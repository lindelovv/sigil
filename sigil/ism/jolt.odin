
package ism

import sigil "sigil:core"
import "lib:jolt"

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

physics : ^jolt.PhysicsSystem
job_sys : ^jolt.JobSystem

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

init_jolt :: proc() {
    jolt.Init()
    job_sys = jolt.JobSystemThreadPool_Create(nil)
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
	physics_system := jolt.PhysicsSystem_Create(&physics_settings)
}

//@(system(requires = [Position], writes = [Velocity])) // something like this would be cool
tick_jolt :: proc() {
    //__ensure(jolt.PhysicsSystem_Update(physics, delta_time, 1, job_sys), "physics error")
}

exit_jolt :: proc() {
    jolt.PhysicsSystem_Destroy(physics)
    jolt.JobSystem_Destroy(job_sys)
    jolt.Shutdown()
}
