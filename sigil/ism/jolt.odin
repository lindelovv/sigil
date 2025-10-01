package ism

import sigil "sigil:core"
import "lib:jolt"
import "core:math"
import "core:math/rand"
import glm "core:math/linalg/glsl"

jolt :: proc(e: sigil.entity_t) -> typeid {
    using sigil
    add(e, name_t("jolt_module"))
    schedule(e, init(init_jolt))
    schedule(e, tick(tick_jolt))
    schedule(e, exit(exit_jolt))
    return none
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

OBJECT_LAYER_NON_MOVING: jolt.ObjectLayer = 0
OBJECT_LAYER_MOVING: jolt.ObjectLayer = 1
//OBJECT_LAYER_PLAYER: jolt.ObjectLayer = 2
OBJECT_LAYER_NUM :: 2

BROAD_PHASE_LAYER_NON_MOVING: jolt.BroadPhaseLayer = 0
BROAD_PHASE_LAYER_MOVING: jolt.BroadPhaseLayer = 1
BROAD_PHASE_LAYER_NUM :: 2

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

physics_system     : ^jolt.PhysicsSystem
job_system         : ^jolt.JobSystem
body_interface     : ^jolt.BodyInterface
narrow_phase_query : ^jolt.NarrowPhaseQuery

floor_id: jolt.BodyID
box_e: sigil.entity_t

physics_id_t :: distinct jolt.BodyID

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

init_jolt :: proc() {
    jolt.Init()
    job_system = jolt.JobSystemThreadPool_Create(nil)
    object_layer_pair_filter := jolt.ObjectLayerPairFilterTable_Create(OBJECT_LAYER_NUM)
	jolt.ObjectLayerPairFilterTable_EnableCollision(
		object_layer_pair_filter,
		OBJECT_LAYER_NON_MOVING,
		OBJECT_LAYER_MOVING,
	)
	jolt.ObjectLayerPairFilterTable_EnableCollision(
		object_layer_pair_filter,
		OBJECT_LAYER_MOVING,
		OBJECT_LAYER_NON_MOVING,
	)
	//jolt.ObjectLayerPairFilterTable_EnableCollision(
	//	object_layer_pair_filter,
	//	OBJECT_LAYER_NON_MOVING,
    //    OBJECT_LAYER_PLAYER,
	//)
	//jolt.ObjectLayerPairFilterTable_EnableCollision(
	//	object_layer_pair_filter,
	//	OBJECT_LAYER_MOVING,
    //    OBJECT_LAYER_PLAYER,
	//)
	broad_phase_layer_interface_table := jolt.BroadPhaseLayerInterfaceTable_Create(
		OBJECT_LAYER_NUM,
		BROAD_PHASE_LAYER_NUM,
	)
	jolt.BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(
		broad_phase_layer_interface_table,
		OBJECT_LAYER_NON_MOVING,
		BROAD_PHASE_LAYER_NON_MOVING,
	)
	jolt.BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(
		broad_phase_layer_interface_table,
		OBJECT_LAYER_MOVING,
		BROAD_PHASE_LAYER_MOVING,
	)
	object_vs_broad_phase_layer_filter := jolt.ObjectVsBroadPhaseLayerFilterTable_Create(
		broad_phase_layer_interface_table,
		BROAD_PHASE_LAYER_NUM,
		object_layer_pair_filter,
		OBJECT_LAYER_NUM,
	)
	physics_settings := jolt.PhysicsSystemSettings {
		maxBodies                     = 65535,
		numBodyMutexes                = 0,
		maxBodyPairs                  = 65535,
		maxContactConstraints         = 65535,
		broadPhaseLayerInterface      = broad_phase_layer_interface_table,
		objectLayerPairFilter         = object_layer_pair_filter,
		objectVsBroadPhaseLayerFilter = object_vs_broad_phase_layer_filter,
	}
	physics_system = jolt.PhysicsSystem_Create(&physics_settings)
	body_interface = jolt.PhysicsSystem_GetBodyInterface(physics_system)
	narrow_phase_query = jolt.PhysicsSystem_GetNarrowPhaseQuery(physics_system)

	jolt.PhysicsSystem_SetGravity(physics_system, &{ 0, 0, -9.8 })
}

tick_jolt :: proc() {
    __ensure(
		jolt.PhysicsSystem_Update(physics_system, delta_time, 1, job_system),
		"physics system update error"
	)
	for &q in sigil.query(physics_id_t, transform_t) {
        id, transform := u32(q.x), (^glm.mat4)(&q.y)
        scale := glm.vec3 { glm.length(transform[0].xyz), glm.length(transform[1].xyz), glm.length(transform[2].xyz) }
        jolt.BodyInterface_GetWorldTransform(body_interface, id, transform)
        transform[0].xyz = glm.normalize(transform[0].xyz) * scale.x
        transform[1].xyz = glm.normalize(transform[1].xyz) * scale.y
        transform[2].xyz = glm.normalize(transform[2].xyz) * scale.z
	}
}

add_physics_shape :: proc(
    entity      : sigil.entity_t,
    pos         : ^glm.vec3,
    rot         : ^glm.quat,
    scl         : ^glm.vec3, 
    motion_type : jolt.MotionType = .JPH_MotionType_Dynamic
) {
    settings: ^jolt.BodyCreationSettings
    if motion_type == .JPH_MotionType_Static {
        shape := jolt.BoxShape_Create(&{1 * scl.x, 1 * scl.y, 0.1}, jolt.DEFAULT_CONVEX_RADIUS)
        settings = jolt.BodyCreationSettings_Create3(cast(^jolt.Shape)shape, pos, rot, motion_type, OBJECT_LAYER_NON_MOVING)
    }
    else {
        shape := jolt.SphereShape_Create(scl.x)
        settings = jolt.BodyCreationSettings_Create3(cast(^jolt.Shape)shape, pos, rot, motion_type, OBJECT_LAYER_MOVING)
    }
    defer jolt.BodyCreationSettings_Destroy(settings)
    id := jolt.BodyInterface_CreateAndAddBody(body_interface, settings, .JPH_Activation_Activate)
    sigil.add(entity, physics_id_t(id))
}

exit_jolt :: proc() {
    jolt.PhysicsSystem_Destroy(physics_system)
    jolt.JobSystem_Destroy(job_system)
    jolt.Shutdown()
}

