
package ism

import sigil "sigil:core"
import "lib:jolt"
import "core:math/rand"
import glm "core:math/linalg/glsl"

// todo: implement all of it :-)

jolt :: proc(e: sigil.entity_t) -> typeid {
    using sigil
    add(e, name("jolt_module"))
    schedule(e, init(init_jolt))
    schedule(e, tick(tick_jolt))
    schedule(e, exit(exit_jolt))
    return none
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

OBJECT_LAYER_NON_MOVING: jolt.ObjectLayer = 0
OBJECT_LAYER_MOVING: jolt.ObjectLayer = 1
OBJECT_LAYER_NUM :: 2

BROAD_PHASE_LAYER_NON_MOVING: jolt.BroadPhaseLayer = 0
BROAD_PHASE_LAYER_MOVING: jolt.BroadPhaseLayer = 1
BROAD_PHASE_LAYER_NUM :: 2

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

physics_system : ^jolt.PhysicsSystem
job_system : ^jolt.JobSystem
body_interface: ^jolt.BodyInterface
narrow_phase_query : ^jolt.NarrowPhaseQuery

floor_id: jolt.BodyID
box_e: sigil.entity_t

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

init_jolt :: proc() {
    jolt.Init()
    job_system = jolt.JobSystemThreadPool_Create(nil)
    object_layer_pair_filter := jolt.ObjectLayerPairFilterTable_Create(OBJECT_LAYER_NUM)
	jolt.ObjectLayerPairFilterTable_EnableCollision(
		object_layer_pair_filter,
		OBJECT_LAYER_MOVING,
		OBJECT_LAYER_MOVING,
	)
	jolt.ObjectLayerPairFilterTable_EnableCollision(
		object_layer_pair_filter,
		OBJECT_LAYER_MOVING,
		OBJECT_LAYER_NON_MOVING,
	)
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

	floor_shape := jolt.BoxShape_Create(&{25, 0.5, 25}, jolt.DEFAULT_CONVEX_RADIUS)

	floor_settings := jolt.BodyCreationSettings_Create3(
		cast(^jolt.Shape)floor_shape,
		&{0, 0, 0},
		nil,
		.JPH_MotionType_Static,
		OBJECT_LAYER_NON_MOVING,
	)
	defer jolt.BodyCreationSettings_Destroy(floor_settings)

	jolt.BodyCreationSettings_SetRestitution(floor_settings, 0.5)
	jolt.BodyCreationSettings_SetFriction(floor_settings, 0.5)

	floor_id = jolt.BodyInterface_CreateAndAddBody(
		body_interface,
		floor_settings,
		.JPH_Activation_DontActivate,
	)
	
	box_shape := jolt.BoxShape_Create(&{ 1, 1, 1 }, jolt.DEFAULT_CONVEX_RADIUS)
	jolt.PhysicsSystem_OptimizeBroadPhase(physics_system)
	
	box_e = sigil.new_entity()

	pos := sigil.add(box_e, position_t {
		rand.float32_range(-22, 22),
		rand.float32_range(20, 30),
		rand.float32_range(-22, 22),
	})
	sigil.add(box_e, rotation_t(0))

	box_settings := jolt.BodyCreationSettings_Create3(
		cast(^jolt.Shape)box_shape,
		cast(^[3]f32)&pos,
		nil,
		.JPH_MotionType_Dynamic,
		OBJECT_LAYER_MOVING,
	)
	defer jolt.BodyCreationSettings_Destroy(box_settings)
	
	id := jolt.BodyInterface_CreateAndAddBody(body_interface, box_settings, .JPH_Activation_Activate)
	sigil.add(box_e, jolt.BodyID(id))
}

//@(system(requires = [Position], writes = [Velocity])) // something like this would be cool
tick_jolt :: proc() {
    __ensure(
		jolt.PhysicsSystem_Update(physics_system, delta_time, 1, job_system),
		"physics system update error"
	)
	for &q in sigil.query(position_t, rotation_t, jolt.BodyID) { // TODO: this proved entity grouping is broken, fix
		pos, rot, id := (^[3]f32)(&q.x), (^quaternion128)(&q.y), q.z
		
		jolt.BodyInterface_GetPosition(body_interface, id, pos)
		jolt.BodyInterface_GetRotation(body_interface, id, rot)

		is_active := jolt.BodyInterface_IsActive(body_interface, id)
	}
	//for &q in sigil.query(position_t, rotation_t, render_data_t) {
	//	pos, rot, data := q.x, q.y, &q.z
	//	data.gpu_data.model = glm.mat4Translate(([3]f32)(pos)) * glm.mat4FromQuat(quaternion128(rot))
	//}
}

exit_jolt :: proc() {
    jolt.PhysicsSystem_Destroy(physics_system)
    jolt.JobSystem_Destroy(job_system)
    jolt.Shutdown()
}
