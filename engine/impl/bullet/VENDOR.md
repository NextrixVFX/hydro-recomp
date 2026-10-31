# Bullet Physics 2.82 - vendored, NOT validated, NOT compiled

Source: github.com/bulletphysics/bullet3, tag `2.82`, commit
`19f999ac087e68ffc2551ffb73e35e60271a0d27` (archive SHA-256
`D0DD628941CCC2B1981F3E916C5382C4756150942B294EA5A365A9722DBEAE2F`).
`BT_BULLET_VERSION 282`. Only `src/LinearMath`, `src/BulletCollision`,
`src/BulletDynamics`, the two `*Common.h` headers and the license are kept;
the CMake/premake files were removed.

Retail statically links **Bullet 2.79** (source path string
`..\VuEngine\Libs\Bullet-2.79\...` in the image). 2.79 predates the git
repository, so 2.82 is the BASE, not the authority. The rule, agreed with
the user:

- every Bullet function the boat's call path enters is diffed against the
  binary and patched to the binary where they differ;
- nothing here is added to a vcxproj or the unity build until its file has
  been diffed;
- `btDiscreteDynamicsWorld::stepSimulation` is NOT taken from here at all -
  retail's VuDynamicsWorldImpl overrides it (@0x574C30) with its own loop.

See memory/audit.md "VU DYNAMICS GLUE READ" for what has been read.

## Patch log (each change cites the retail evidence; marked `hydro-recomp PATCH` in source)

1. `btRigidBody::setCenterOfMassTransform` - `isKinematicObject()` ->
   `isStaticOrKinematicObject()`. Retail @0x50E660 tests `flags & 3`.

2. `btDiscreteDynamicsWorld::predictUnconstraintMotion` - calls
   `integrateVelocities` before `applyDamping`, as retail @0x6031B0 does.
   **Dependency:** the 2.82 solver integrates forces itself; that must be
   removed when the solver is diffed or forces apply twice.
3. `btDiscreteDynamicsWorld::internalSingleStepSimulation` - no
   `createPredictiveContacts` call (retail @0x600E80).

## Proven differences NOT yet patched

- **The solver's external-force integration does not exist in 2.79.** A
  scan of every function the retail solver calls directly found no read of
  a body's totalForce/totalTorque (control: the same scan finds them in
  integrateVelocities and applyGravity). 2.82's m_externalForceImpulse /
  m_externalTorqueImpulse in btSolverBody and btSequentialImpulseConstraint
  Solver must come out with the solver patch - patch 2 depends on it.
- **The 2.79 solver is a different design and is NOT patched from 2.82.**
  Retail accumulates the solver's velocity deltas inside btRigidBody
  (rb+576 / +592) and writes them back with btRigidBody::
  internalWritebackVelocity (@0x574AC0); 2.82 copies velocities into
  btSolverBody and writes back from there. btSequentialImpulseConstraint
  Solver.cpp and btSolverBody.h will be recreated from the binary, using
  this 2.82 source only as a reading aid.
- The world vtable: retail VuDynamicsWorldImpl has 42 slots; 2.82 adds
  virtuals (createPredictiveContacts at least). Slot-by-slot layout not yet
  compared.

- **Rolling friction does not exist in 2.79.** Retail's
  btRigidBodyConstructionInfo reads sleeping thresholds at +128/+132
  (2.82: +132/+136) and btCollisionObject's m_internalType sits at +244
  (2.82: +248). Removing it touches btCollisionObject.h/.cpp (member,
  setter/getter, serialisation), btRigidBody.h/.cpp (cinfo member, ctor
  default, setupRigidBody), btManifoldPoint (m_combinedRollingFriction),
  btManifoldResult (calculateCombinedRollingFriction) and
  btSequentialImpulseConstraintSolver (the rolling-friction constraint
  pool). Patched as ONE change once the manifold and solver files are
  diffed, so every removal has its own retail evidence.
