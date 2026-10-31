// NOT COMPILED. Not in hydro.vcxproj, not in the unity build.
//
// The only thing here worth keeping is the KEY NAMES, which came from the
// authored project data: VuAIUIDriverStartEntity places a grid start, and
// VuAiWaypointEntity carries "Speed Hint", "CheckPoint Width", "Start" and
// "Finish".
//
// The AI itself was entirely INFERRED and is gone: a waypoint ring walked by
// nearest-gate lookup, a steering controller with invented slow/fast turn
// bands, and the traffic integrator. The defaults below (80 mph speed hint,
// 12 unit checkpoint width) are inventions, not retail's - do not reuse them.
// Take the schema and the defaults from each entity's constructor.

            if ( type->text == "VuAIUIDriverStartEntity" )
            {
                driver_t driver{};
                driver.x = world[ 12 ];
                driver.y = world[ 13 ];
                driver.fx = world[ 4 ];
                driver.fy = world[ 5 ];
            }
            if ( type->text == "VuAiWaypointEntity" && properties != nullptr )
            {
                gate_t gate{};
                gate.x = world[ 12 ];
                gate.y = world[ 13 ];
                gate.fx = world[ 4 ];
                gate.fy = world[ 5 ];
                gate.speed = number( find( *properties, "Speed Hint" ), 80.0f );
                gate.width = number( find( *properties, "CheckPoint Width" ), 12.0f );
                gate.start = number( find( *properties, "Start" ), 0.0f ) != 0.0f;
                gate.finish = number( find( *properties, "Finish" ), 0.0f ) != 0.0f;
            }
