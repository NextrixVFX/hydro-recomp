#pragma once

#include <workspace/core/dynamics/island.hxx>
#include <workspace/core/dynamics/world.hxx>

#include <vector>

// The world step, recreated from retail: VuDynamicsWorldImpl's
// stepSimulation @0x574C30 (the only btDiscreteDynamicsWorld slot Vu
// overrides - slot 11) over btDiscreteDynamicsWorld::internalSingleStep
// Simulation @0x600E80. Retail reaches every stage through the world's
// vtable (0x757A94); the stages here are a table with the same entries,
// defaulting to the pieces recreated and verified in this directory.
//
// NOT recreated, and therefore hooks the caller supplies:
//  - performDiscreteCollisionDetection (slot 9, 0x54C710): the broadphase
//    and the dispatcher. It must leave world.manifolds / world.pairs as
//    retail's would.
//  - every call into Vu code: the internal pre-tick / tick callbacks (world
//    +92 / +88), the actions (+252 / +260, each updateAction(world, dt)),
//    the per-substep callback object (+272: slot 0 (dt, bool), slot 1
//    (dt)), the motion states' getWorldTransform, the debug drawer's mode.
// Retail wraps the stages in CProfileSamples; timing only, not recreated.

struct world_t;
struct collision_world_t;

struct world_stages_t
{
    void ( *predict )( world_t& w, float dt );              // slot 33  predictUnconstraintMotion
    void ( *collide )( world_t& w );                        // slot 9   performDiscreteCollisionDetection (HOOK)
    void ( *islands )( world_t& w );                        // slot 35  calculateSimulationIslands
    bool ( *solve )( world_t& w );                          // slot 36  solveConstraints(world +100)
    bool ( *integrate )( world_t& w, float dt );            // slot 34  integrateTransforms
    void ( *actions )( world_t& w, float dt );              // 0x601530 updateActions (Vu actions - HOOK)
    void ( *activation )( world_t& w, float dt );           // 0x601640 updateActivationState
    void ( *save_kinematic )( world_t& w, float dt );       // slot 38  saveKinematicState
    void ( *gravity )( world_t& w );                        // slot 39  applyGravity
    bool ( *internal_step )( world_t& w, float dt );        // slot 37  internalSingleStepSimulation
    void ( *clear_forces )( world_t& w );                   // slot 28  clearForces
};

// The Vu callbacks, all optional.
struct world_hooks_t
{
    void* user = nullptr;
    void ( *pre_tick )( void* user, world_t& w, float dt ) = nullptr;      // world +92
    void ( *tick )( void* user, world_t& w, float dt ) = nullptr;          // world +88
    bool has_substep_callback = false;                                     // world +272 non-null
    void ( *substep_begin )( void* user, float dt, bool stepping ) = nullptr;  // +272 slot 0
    void ( *substep )( void* user, float dt ) = nullptr;                       // +272 slot 1
    bool has_debug_drawer = false;                                         // world +80 non-null
    int debug_mode = 0;                                                    // the drawer's slot 11 (vt+44)
    body_motion_fn motion = nullptr;                                       // motion states' getWorldTransform
};

struct world_t
{
    std::vector< solver_rigid_t* > objects;         // +8 / +16 the collision objects
    std::vector< char > has_motion;                 // per object: body +500 non-null
    std::vector< solver_rigid_t* > bodies;          // +204 / +212 the non-static rigid bodies
    std::vector< int > shape_types;                 // per body: shape (+204) type (+4)
    std::vector< const island_constraint_t* > constraints;  // +184 / +192
    std::vector< solver_manifold_t* > manifolds;    // the dispatcher's, written by `collide`
    std::vector< island_pair_t > pairs;             // the broadphase's, written by `collide`
    island_manager_t island_manager;                // +176
    solver_t solver;                                // +172
    solver_info_t info = world_solver_info( );      // +100
    float dispatch_time_step = 0.0f;                // +28
    int dispatch_step_count = 0;                    // +32
    bool use_continuous = true;                     // +44 (btCollisionWorld ctor default)
    bool force_update_all_aabbs = true;             // +84 (btCollisionWorld ctor 0x54BFE0 default)
    float gravity[ 4 ] = { 0.0f, -10.0f, 0.0f, 0.0f };  // +224 (the world ctor 0x5FFF40's DEFAULT)
    collision_world_t* collision = nullptr;         // the collision side (collision/world.hxx)
    float local_time = 0.0f;                        // +240 (VuDynamicsWorldImpl)
    bool disable_deactivation = false;              // gDisableDeactivation, .bss 0x758FFF
    world_hooks_t hooks;
    world_stages_t stages;
    bool refused = false;   // set once a stage refused a path not recreated

    world_t( );     // installs world_default_stages()
};

inline void world_stage_predict( world_t& w, float dt )
{
    world_predict_unconstraint_motion( w.bodies.data( ), static_cast< int >( w.bodies.size( ) ), dt );
}

inline void world_stage_collide( world_t& )
{
    // performDiscreteCollisionDetection is not recreated: a world that
    // needs it installs its own stage. The manifolds and pairs stay as given.
}

inline void world_stage_islands( world_t& w )
{
    island_calculate( w.island_manager, w.objects.data( ), static_cast< int >( w.objects.size( ) ), w.pairs.data( ),
        static_cast< int >( w.pairs.size( ) ), w.constraints.data( ), static_cast< int >( w.constraints.size( ) ) );
}

inline bool world_stage_solve( world_t& w )
{
    return island_solve_constraints( w.solver, w.info, w.island_manager, w.objects.data( ),
        static_cast< int >( w.objects.size( ) ), w.manifolds.data( ), static_cast< int >( w.manifolds.size( ) ),
        w.constraints.data( ), static_cast< int >( w.constraints.size( ) ) );
}

inline bool world_stage_integrate( world_t& w, float dt )
{
    return world_integrate_transforms( w.bodies.data( ), w.shape_types.data( ), static_cast< int >( w.bodies.size( ) ), dt,
        w.use_continuous );
}

inline void world_stage_actions( world_t&, float )
{
    // updateActions @0x601530 calls each action's updateAction - Vu code.
    // None is recreated; a world with actions installs its own stage.
}

inline void world_stage_activation( world_t& w, float dt )
{
    world_update_activation_state( w.bodies.data( ), static_cast< int >( w.bodies.size( ) ), dt, w.disable_deactivation );
}

inline void world_stage_save_kinematic( world_t& w, float dt )
{
    world_save_kinematic_state( w.objects.data( ), w.has_motion.data( ), static_cast< int >( w.objects.size( ) ), dt,
        w.hooks.motion, w.hooks.user );
}

inline void world_stage_gravity( world_t& w )
{
    world_apply_gravity( w.bodies.data( ), static_cast< int >( w.bodies.size( ) ) );
}

inline void world_stage_clear_forces( world_t& w )
{
    world_clear_forces( w.bodies.data( ), static_cast< int >( w.bodies.size( ) ) );
}

// internalSingleStepSimulation @0x600E80 (slot 37; ecx world, dt on the
// stack), in retail's order: pre-tick callback (+92); predict (slot 33);
// dispatchInfo timeStep (+28) = dt, stepCount (+32) = 0, debugDraw (+48) =
// the drawer; collision (slot 9); islands (slot 35); solverInfo timeStep
// (+112) = dt; solveConstraints (slot 36, info +100); integrateTransforms
// (slot 34); updateActions; updateActivationState; tick callback (+88).
// Returns false if a stage refused a path not recreated.
inline bool world_internal_step( world_t& w, float dt )
{
    if ( w.hooks.pre_tick )
        w.hooks.pre_tick( w.hooks.user, w, dt );
    w.stages.predict( w, dt );
    w.dispatch_time_step = dt;
    w.dispatch_step_count = 0;
    w.stages.collide( w );
    w.stages.islands( w );
    w.info.time_step = dt;
    bool ok = w.stages.solve( w );
    ok = w.stages.integrate( w, dt ) && ok;
    w.stages.actions( w, dt );
    w.stages.activation( w, dt );
    if ( w.hooks.tick )
        w.hooks.tick( w.hooks.user, w, dt );
    return ok;
}

inline world_stages_t world_default_stages( )
{
    world_stages_t s;
    s.predict = world_stage_predict;
    s.collide = world_stage_collide;
    s.islands = world_stage_islands;
    s.solve = world_stage_solve;
    s.integrate = world_stage_integrate;
    s.actions = world_stage_actions;
    s.activation = world_stage_activation;
    s.save_kinematic = world_stage_save_kinematic;
    s.gravity = world_stage_gravity;
    s.internal_step = world_internal_step;
    s.clear_forces = world_stage_clear_forces;
    return s;
}

inline world_t::world_t( ) : stages( world_default_stages( ) ) {}

// VuDynamicsWorldImpl::stepSimulation @0x574C30 (ecx world; timeStep,
// maxSubSteps, fixedTimeStep on the stack; retn 0Ch), from the
// disassembly:
//   local = local + timeStep; n = 0
//   if local > fixed (ordered):  n = (int)(local / fixed)  (cvttss2si,
//                                truncating);  local = local - float(n) * fixed
//   if a debug drawer:  gDisableDeactivation = (mode >> 4) & 1
//   if n:  saveKinematicState(fixed) ONCE; then min(n, maxSubSteps) times:
//          applyGravity; callback slot 0 (first substep: fixed - the local
//          time BEFORE this call's add, else fixed; true) and slot 1
//          (fixed); internalSingleStepSimulation(fixed); clearForces
//   callback slot 0 (n ? local : timeStep, false)
// Returns n - the UNCLAMPED count. No synchronizeMotionStates here.
inline int world_step_simulation( world_t& w, float time_step, int max_sub_steps, float fixed_time_step )
{
    const float old_local = w.local_time;
    float local = old_local + time_step;
    w.local_time = local;
    int n = 0;
    if ( local > fixed_time_step )
    {
        n = static_cast< int >( local / fixed_time_step );
        local = local - static_cast< float >( n ) * fixed_time_step;
        w.local_time = local;
    }
    if ( w.hooks.has_debug_drawer )
        w.disable_deactivation = ( ( w.hooks.debug_mode >> 4 ) & 1 ) != 0;
    if ( n )
    {
        w.stages.save_kinematic( w, fixed_time_step );
        const int clamped = n > max_sub_steps ? max_sub_steps : n;
        for ( int i = 0; i < clamped; ++i )
        {
            w.stages.gravity( w );
            if ( w.hooks.has_substep_callback )
            {
                const float first = i == 0 ? fixed_time_step - old_local : fixed_time_step;
                if ( w.hooks.substep_begin )
                    w.hooks.substep_begin( w.hooks.user, first, true );
                if ( w.hooks.substep )
                    w.hooks.substep( w.hooks.user, fixed_time_step );
            }
            if ( !w.stages.internal_step( w, fixed_time_step ) )
                w.refused = true;
            w.stages.clear_forces( w );
        }
    }
    if ( w.hooks.has_substep_callback && w.hooks.substep_begin )
        w.hooks.substep_begin( w.hooks.user, n ? w.local_time : time_step, false );
    return n;
}
