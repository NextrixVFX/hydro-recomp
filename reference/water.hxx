// NOT COMPILED. Not in hydro.vcxproj, not in the unity build.
//
// VuWaterSurfaceEntity, partially read. The property KEYS below are real -
// they were read out of the authored project files, not guessed - and
// Hydroelectric authors 13 of these entities, each with its own transform and
// height. That much is worth keeping.
//
// Everything that consumed them was not: the surface was cached with a
// normalised basis and half extents, the height under a point was resolved by
// picking the smallest overlapping RECTANGLE, and the "widest surface wins"
// rule that seeded the single-surface fields was invented outright. Retail
// resolves a water height through the physics world. All of that is deleted
// rather than banked.
//
// The entity has 14 authored properties; this reads 7. Build it properly from
// VuWaterSurfaceEntity's constructor for the schema and defaults, and from its
// vtable methods for the geometry and height resolution.

            if ( type->text == "VuWaterSurfaceEntity" && properties != nullptr )
            {
                surface_t surface{};
                std::memcpy( surface.world, world, sizeof( surface.world ) );
                surface.width = number( find( *properties, "X Size" ), 1024.0f );
                surface.depth = number( find( *properties, "Y Size" ), 1024.0f );
                const value_t* map = find( *properties, "WaterMap" );
                if ( map != nullptr && map->kind == value_t::kind_t::text )
                    surface.map = map->text;
                const value_t* diffuse = find( *properties, "DiffuseColor" );
                const value_t& source = diffuse != nullptr ? *diffuse : *properties;
                surface.color[ 0 ] = number( find( source, "R" ), 60.0f ) / 255.0f;
                surface.color[ 1 ] = number( find( source, "G" ), 80.0f ) / 255.0f;
                surface.color[ 2 ] = number( find( source, "B" ), 90.0f ) / 255.0f;
                surface.color[ 3 ] = number( find( source, "A" ), 102.0f ) / 255.0f;
            }
