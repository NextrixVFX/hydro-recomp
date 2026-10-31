#pragma once

// c_world's graph - member definitions moved out of world.hxx (one class, split by concern).

// ---- the event graph -------------------------------------------------
//
// RETAIL MODEL, and it is small. VuScriptOutputPlug_fire @0x5023a0 is the
// WHOLE dispatcher in 138 bytes, reached as vtable slot 2 through the
// thunk @0x503270 after VuScriptComponent_findPlug @0x504770 looks the
// plug up by name. Connections are resolved ONCE at load - sub_503d70 and
// sub_503ec0 read Plugs / Connections / RefConnections, sub_502720
// resolves each EntityName + PlugName pair - and firing then just walks
// the resolved list calling slot 2 on every entry. this[20] / this[21]
// are the array bounds.
//
// this[9] == 0 is a BROADCAST: call every connection and discard the
// results. Non-zero calls ONLY THE FIRST and propagates its return value.
// this[9] is set from the THIRD ARGUMENT of VuScriptOutputPlug_init
// @0x502080, so it is the plug's declared value type and only a
// constructor can tell us which plugs have one - see
// plug_returns_value() below.
//
// DIRECTION IS NOT IN THE DATA. The authored file records a connection on
// BOTH ends, so an input plug carries a mirrored entry pointing back at
// its source. Which end is which comes from the owning entity's
// CONSTRUCTOR: an INPUT plug is built by VuScriptInputPlug_init @0x5e28f0
// (which binds a VuMethod1 handler at plug+96), an OUTPUT plug by
// VuScriptOutputPlug_init. Every classification below cites the ctor that
// settled it, and because we only ever fire names those ctors prove to be
// outputs, the mirrored input-side entries are never walked.
inline void c_world::collect_graph( const std::string& owner, const value_t* components )
{
    const value_t* script = components != nullptr
        ? find( *components, "VuScriptComponent" ) : nullptr;
    if ( script == nullptr )
        return;
    // THE REFS. A VuScriptComponent carries more than plugs: every
    // VuMotionEntity declares a VuScriptRef named "Entity"
    // (VuMotionEntity_ctor @0x61a000 stores the literal at ref+12), and
    // VuMotionEntity_onActivate @0x61a4f0 drives whatever that ref
    // resolves to. VuAttachEntity's 30 instances need the same thing.
    // The authored side is read by sub_503d70 / sub_503ec0 as
    // "Refs" / "RefConnections"; this probe prints the real shape once so
    // the parser is written against the data rather than against a guess.
    // THE REFS, measured off the asset rather than guessed: a
    // VuScriptComponent's "Refs" node is a flat map from the ref's NAME to
    // the FULL PATH of the entity it points at, as one text value.
    //
    //   Refs { "Child":  "Hydroelectric/Audio/AudioEmitter_Helicopter",
    //          "Parent": "Hydroelectric/Terrain/zone06 .../Helicopter" }
    //
    // A ref is declared in a constructor as a VuScriptRef carrying that
    // name - VuMotionEntity_ctor @0x61a000 builds one called "Entity" and
    // VuMotionEntity_onActivate @0x61a4f0 drives whatever it resolves to.
    // This is the block that VuAttachEntity's 30 instances need, and the
    // reason the windmills have no propellers.
    const value_t* refs = find( *script, "Refs" );
    if ( refs != nullptr && refs->kind == value_t::kind_t::object )
    {
        for ( const auto& named : refs->object )
        {
            if ( named.second.kind != value_t::kind_t::text || named.second.text.empty( ) )
                continue;
            m_refs.emplace( std::make_pair( owner, named.first ), named.second.text );
        }
    }
    const value_t* plugs = find( *script, "Plugs" );
    if ( plugs == nullptr || plugs->kind != value_t::kind_t::object )
        return;
    for ( const auto& named : plugs->object )
    {
        const value_t* links = find( named.second, "Connections" );
        if ( links == nullptr || links->kind != value_t::kind_t::list || links->list.empty( ) )
            continue;
        std::vector< edge_t > edges;
        for ( const value_t& link : links->list )
        {
            const value_t* entity = find( link, "EntityName" );
            const value_t* port = find( link, "PlugName" );
            if ( entity == nullptr || entity->kind != value_t::kind_t::text || entity->text.empty( ) )
                continue;
            edge_t edge{};
            edge.entity = entity->text;
            if ( port != nullptr && port->kind == value_t::kind_t::text )
                edge.plug = port->text;
            edges.push_back( std::move( edge ) );
        }
        if ( edges.empty( ) )
            continue;
        // An entity reached through a template expansion can be walked
        // more than once; keeping the first resolution stops one plug
        // accumulating the same connection twice and firing twice.
        m_graph.emplace( std::make_pair( owner, named.first ), std::move( edges ) );
    }
}

// Which plugs carry a VALUE. this[9] in VuScriptOutputPlug_fire is set
// from the THIRD ARGUMENT of VuScriptOutputPlug_init @0x502080
// (`this[9] = a3`), and every ctor read so far passes 0 except two:
//
//   VuIfEntity_ctor      @0x5ddbe0   "Operand", 3
//   VuBooleanEntity_ctor @0x5e5880   "Get",     3
//
// and 3 is exactly the tag VuBoolean_onGet returns. So this[9] is the
// plug's RETURN TYPE, 0 meaning void - which is what selects the
// dispatcher's two halves: a void plug broadcasts to every connection and
// discards the results, a typed plug calls ONLY THE FIRST and propagates
// its return value. Nothing in the authored data says which is which;
// only the constructor does, so this list grows one ctor at a time.
inline bool c_world::plug_returns_value( const std::string& type, const std::string& plug )
{
    return ( type == "VuIfEntity" && plug == "Operand" )
        || ( type == "VuBooleanEntity" && plug == "Get" );
}

// VuScriptOutputPlug_fire @0x5023a0, both halves.
inline c_world::retval_t c_world::fire( const std::string& owner, const std::string& plug, int depth)
{
    const auto found = m_graph.find( std::make_pair( owner, plug ) );
    if ( found == m_graph.end( ) || found->second.empty( ) )
        return retval_t{};
    const auto type = m_entity_type.find( owner );
    if ( type != m_entity_type.end( ) && plug_returns_value( type->second, plug ) )
        return dispatch( found->second.front( ).entity, found->second.front( ).plug, depth + 1 );
    // The handler may edit m_graph-adjacent state but never m_graph
    // itself, so iterating the resolved list directly is safe.
    for ( const edge_t& edge : found->second )
        dispatch( edge.entity, edge.plug, depth + 1 );
    return retval_t{};
}

// Slot 2 on an INPUT plug: invoke the VuMethod1 its entity's constructor
// bound to it. Each branch below is one such handler, transliterated.
// DEVIATION, FLAGGED: the depth guard. Retail has none - VuOneShotFilter's
// In handler even sets its latch AFTER firing Out, so a cyclic graph
// recurses forever there too - and nothing in the authored graph is known
// to be cyclic, so the guard only bounds a bug.
inline c_world::retval_t c_world::dispatch( const std::string& entity, const std::string& plug, int depth)
{
    if ( depth > 8 )
        return retval_t{};

    // VuPlayAnimationEntity. INPUT plug "Trigger", built by
    // VuPlayAnimationEntity_initTriggerPlug @0x5f6240 and handled by
    // VuPlayAnimationEntity_onTriggerPlug @0x5f5e80:
    //
    //   if ( !this->done )                       // byte +168
    //   {
    //       if ( this->oneShot ) this->done = 1; // byte +157
    //       if ( animationAsset->loaded )        // *(ref+76)
    //           fire( "PlayAnimation", params );
    //   }
    //   return VuRetVal( 0 );
    //
    // The params it builds carry the asset, Start Time (+144), Blend Time
    // (+148), Time Factor (+152) and Looping (+156): the PROP owns none of
    // that, which is why our play_t is indexed by the target's path and
    // copied onto the prop in bind_animations.
    // VuAttachEntity's two input plugs, @0x5f5550 and @0x5f5690. Attach
    // resolves both refs, builds the relative matrix and hands it to the
    // parent's attach component; Detach calls the component's slot 13.
    // Both return VuRetVal( 0 ). A second Attach on a claimed child is
    // refused by the component, which is what `attached` models.
    for ( attach_t& join : m_attaches )
    {
        if ( join.path != entity )
            continue;
        if ( plug == "Attach" )
        {
            join.attached = true;
            update_attaches( );
            return retval_t{};
        }
        if ( plug == "Detach" )
        {
            join.attached = false;
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuCoronaEntity's two input plugs. @0x5e8be0 and @0x5e8c30 register
    // and unregister the corona's draw component with the global
    // Vu3dDrawManager, guarded by the same +240 flag, and both return
    // VuRetVal( 0 ). A redundant Show or Hide does nothing at all.
    if ( m_coronas.owns( entity ) )
    {
        if ( plug == "Show" )
        {
            m_coronas.show( entity );
            return retval_t{};
        }
        if ( plug == "Hide" )
        {
            m_coronas.hide( entity );
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuSetFrontEndCameraEntity. "Trigger" is its ONE input plug, named
    // inside the init helper @0x409570 which binds
    // VuMethod1<VuSetFrontEndCameraEntity,VuRetVal,VuParams const&> to the
    // handler @0x408f90. That handler, entire:
    //
    //   if ( this->modifyEyePosition )    { params.add( eye    ); broadcast( "OnSetFrontEndCameraEyePosition",    params ); }
    //   if ( this->modifyTargetPosition ) { params.add( target ); broadcast( "OnSetFrontEndCameraTargetPosition", params ); }
    //   if ( this->modifyFarPlane )       { params.add( far    ); broadcast( "OnSetFrontEndCameraDrawDistance",   params ); }
    //                                       params.add( lerp   ); broadcast( "OnSetFrontEndCameraInterpolation",  params );
    //   return VuRetVal( 0 );
    //
    // Four separate engine-wide broadcasts, each name hashed with FNV-1a
    // (basis 0x811c9dc5, prime 16777619) and each carrying exactly ONE
    // parameter - the param count is re-zeroed before every add, so the
    // buffer is reused but never accumulates. The interpolation broadcast
    // is UNCONDITIONAL: there is no Modify boolean for it.
    //
    // COVERAGE GAP, NAMED: the four subscribers live on the front-end
    // camera, which needs the menus. Nothing in the build hooks
    // "OnSetFrontEndCamera*" yet, so the eye, target and far plane are
    // applied straight to the scene viewer - OUR vantage, not retail's -
    // and Interpolation Time is carried but not acted on, because no
    // interpolation code has been read.
    const auto shot = m_front_end.find( entity );
    if ( shot != m_front_end.end( ) )
    {
        if ( plug == "Trigger" )
        {
            const front_end_camera_t& set = shot->second;
            if ( set.modify_eye )
                std::memcpy( m_eye, set.eye, sizeof( m_eye ) );
            if ( set.modify_target )
                std::memcpy( m_target, set.target, sizeof( m_target ) );
            if ( set.modify_far )
                m_far = set.far_plane;
            if ( set.modify_eye || set.modify_target || set.modify_far )
                m_authored_camera = true;
            ++m_fired;
            LOG_SUCCESS( "event: %s set the front end camera  eye %s target %s far %s  interpolation %.3f (carried, not applied)",
                entity.c_str( ),
                set.modify_eye ? "yes" : "no",
                set.modify_target ? "yes" : "no",
                set.modify_far ? "yes" : "no",
                set.interpolation_time );
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    const auto action = m_actions.find( entity );
    if ( action != m_actions.end( ) )
    {
        if ( plug == "Trigger" )
        {
            if ( action->second.done )
                return retval_t{};
            if ( action->second.one_shot )
                action->second.done = true;
            fire( entity, "PlayAnimation", depth );
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // The receiving end of a "PlayAnimation" edge: VuAnimatedPropEntity's
    // INPUT plug "PlayAnimation", bound in its ctor @0x5ec300 to the
    // handler @0x5ecb60. That handler builds a NEW VuAnimationControl on
    // every call, so a prop that is already playing restarts - the old
    // `|| item.playing` skip here was invented. It then:
    //
    //   control+28 = Start Time; advance( 0 )          a no-op
    //   control+24 = Time Factor; +20 = Looping; advance( 0 )
    //   Blend Time <= 0 ? clear the skeleton's controls @0x517440
    //                   : this+126 = 1, this+128 = 1/Blend, weight 0
    //   add the control @0x5173d0; +36 = timed events; +40 = the prop
    //   advance( sixth param )                         0 from this sender
    //   fire( "OnAnimStart" )
    //
    // UNKNOWN, not reproduced: Blend Time. Its weight ramp lives in the
    // prop's tick @0x5ed030 and the pose blend in @0x516cf0; with a single
    // control the blend normalises by the control's own weight, so it
    // only shows as the bind pose until the first tick - but the case of
    // a prop with a previous clip still fading has not been read.
    bool started = false;
    for ( animated_t& item : m_animated )
    {
        if ( item.path != entity )
            continue;
        item.playing = true;
        // control.end / control.loop are the clip's own members, so they
        // are read from the clip where it is advanced, in skin_props().
        item.control.looping = item.looping;
        item.control.factor = item.factor;
        item.control.time = item.start;
        const auto timed = m_timed.find( item.timed );
        item.control.events = timed != m_timed.end( ) ? &timed->second : nullptr;
        ++m_fired;
        started = true;
        fire( entity, "OnAnimStart", depth );
        // The distance proves the ordering: a trigger that only fires
        // after the player has travelled cannot have fired at t = 0 on
        // the start grid.
        LOG_SUCCESS( "event: %s . %s fired after %.0f units driven, %s begins its clip",
            entity.c_str( ), plug.c_str( ), m_triggers.driven( ), item.asset.c_str( ) );
    }
    if ( started )
        return retval_t{};

    // VuPointWaveEntity. "Emit" is the INPUT plug, named inside
    // VuPointWaveEntity_initEmitPlug @0x5d4e60; "OnEmit" and "OnExpired"
    // are outputs the entity fires, so only "Emit" may start a ripple.
    const auto ripple = m_ripple_sources.find( entity );
    if ( ripple != m_ripple_sources.end( ) )
    {
        if ( plug == "Emit" )
        {
            const c_ocean::ripple_t& source = ripple->second;
            m_ocean.emit( source.x, source.y, source.falloff, source.magnitude,
                source.range_start, source.range_end, source.speed, source.frequency,
                source.foaminess );
            ++m_fired;
            LOG_SUCCESS( "event: %s emitted a point wave at %.0f %.0f  range %.0f speed %.0f",
                entity.c_str( ), source.x, source.y, source.range_end, source.speed );
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuOneShotFilterEntity, all three handlers over the byte at this+96.
    const auto latch = m_filters.find( entity );
    if ( latch != m_filters.end( ) )
    {
        // VuOneShotFilter_onResetPlug @0x5decc0: this[96] = 0.
        if ( plug == "Reset" )
        {
            latch->second.fired = false;
            return retval_t{};
        }
        // VuOneShotFilter_onSetShotPlug @0x5dece0: this[96] = 1. It arms
        // the latch as ALREADY SHOT, so a later "In" does nothing - the
        // opposite of Reset, and nothing in the name says so.
        if ( plug == "SetShot" )
        {
            latch->second.fired = true;
            return retval_t{};
        }
        // VuOneShotFilter_onInPlug @0x5df000: if the byte is clear, fire
        // "Out" and THEN set it. The order is retail's, not a tidy-up.
        if ( plug == "In" )
        {
            if ( latch->second.fired )
                return retval_t{};
            fire( entity, "Out", depth );
            latch->second.fired = true;
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuTriggerEntity's own two INPUT plugs, declared by its ctor
    // @0x61c6a0 through VuScriptInputPlug_create @0x61cd90 and shared by
    // the plane and the box. VuTriggerEntity_onActivate @0x61ccb0 and
    // _onDeactivate @0x61cd20 flip the byte at this+132, which is what
    // puts the trigger into (or takes it out of) the manager's active
    // list; a trigger that is not in the list is never tested. Both guard
    // on the current state, so a redundant Activate does nothing.
    if ( plug == "Activate" || plug == "Deactivate" )
    {
        if ( m_triggers.set_active( entity, plug == "Activate" ) )
            return retval_t{};
    }
    else
    {
        for ( const trigger_t& gate : m_triggers.all( ) )
        {
            if ( gate.path != entity )
                continue;
            count_unhandled( entity, plug );
            return retval_t{};
        }
    }

    // VuDelayEntity. VuDelayEntity_onInPlug @0x5dfbe0:
    //
    //   if ( !this->pending )               // byte +104
    //   {
    //       this->pending  = 1;
    //       this->remaining = rand01() * randomWindow + delay;  // +108
    //       copy the incoming params into this+112 for the Out
    //   }
    //   return VuRetVal( 0 );
    //
    // A delay already counting down IGNORES a second In - the retrigger
    // is dropped, not restarted, and that is not what the name suggests.
    const auto wait = m_delays.find( entity );
    if ( wait != m_delays.end( ) )
    {
        if ( plug == "In" )
        {
            if ( !wait->second.pending )
            {
                wait->second.pending = true;
                wait->second.remaining = m_random.draw( ) * wait->second.window
                    + wait->second.delay;
            }
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuGenericEventEntity. The "Broadcast" input handler @0x5f0330 posts
    // the global "OnGenericEvent" carrying this entity's Event Name, and
    // every instance's hook @0x5f0420 fires its own "OnReceived" when the
    // carried name equals its own. Walking the name map IS that
    // comparison; the sender matches itself too, exactly as retail does.
    const auto event = m_events.find( entity );
    if ( event != m_events.end( ) )
    {
        if ( plug == "Broadcast" )
        {
            for ( const auto& pair : m_events )
            {
                if ( pair.second == event->second )
                    fire( pair.first, "OnReceived", depth );
            }
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuIfEntity, ctor @0x5ddbe0. INPUT "Trigger" (named in the init
    // helper @0x5e2670, handler VuIfEntity_onTriggerPlug @0x5dde60),
    // OUTPUTS "Operand" (return type 3), "True" and "False".
    //
    //   ret = fire( "Operand" );
    //   if ( ret.type == 3 ) fire( ret.value ? "True" : "False", params );
    //
    // The TYPE CHECK IS LOAD-BEARING: an Operand that answers with
    // anything other than a bool - including an unconnected Operand -
    // takes neither branch. It does not fall through to False.
    if ( m_ifs.count( entity ) != 0 )
    {
        if ( plug == "Trigger" )
        {
            const retval_t answer = fire( entity, "Operand", depth );
            if ( answer.type == 3 )
                fire( entity, answer.value ? "True" : "False", depth );
            return retval_t{};
        }
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // VuBooleanEntity, ctor @0x5e5880. One property, "Value", on the byte
    // at +96. INPUTS "Set", "SetTrue" (@0x5e5790), "SetFalse" (@0x5e57b0),
    // "Toggle" (@0x5e57d0), "Get" (@0x5e57f0, return type 3); OUTPUTS
    // "OnChanged", "OnChangedTrue", "OnChangedFalse".
    const auto boolean = m_booleans.find( entity );
    if ( boolean != m_booleans.end( ) )
    {
        // VuBoolean_onGet: returns VuRetVal{ 3, value }. This is the only
        // handler in the engine so far that answers rather than acts.
        if ( plug == "Get" )
        {
            retval_t answer{};
            answer.type = 3;
            answer.value = boolean->second;
            return answer;
        }
        if ( plug == "SetTrue" )
            return set_boolean( entity, true, depth );
        if ( plug == "SetFalse" )
            return set_boolean( entity, false, depth );
        // VuBoolean_onToggle: setValue( value == 0 ).
        if ( plug == "Toggle" )
            return set_boolean( entity, !boolean->second, depth );
        // "Set" takes its value from the incoming VuParams, which our
        // events do not carry. Counted, not guessed - and nothing in
        // Hydroelectric is wired to it.
        count_unhandled( entity, plug );
        return retval_t{};
    }

    // Everything else in the 813-node graph - keyframe motion, audio
    // emitters, pfx emitters, the other wave emitters - is not built yet.
    // Count it and say so once, so the gap stays visible instead of
    // events vanishing silently.
    count_unhandled( entity, plug );
    return retval_t{};
}

// VuBooleanEntity_setValue @0x5e5ea0. It fires NOTHING when the value is
// unchanged - the whole body sits inside `if ( a2 != this[96] )` - and
// when it does change it fires "OnChanged" first and then exactly one of
// "OnChangedTrue" / "OnChangedFalse".
inline c_world::retval_t c_world::set_boolean( const std::string& entity, bool value, int depth )
{
    const auto found = m_booleans.find( entity );
    if ( found == m_booleans.end( ) || found->second == value )
        return retval_t{};
    found->second = value;
    fire( entity, "OnChanged", depth );
    fire( entity, value ? "OnChangedTrue" : "OnChangedFalse", depth );
    return retval_t{};
}

// VuDelayEntity's countdown. VuDelayEntity_tick @0x5dfb50 is the method
// registered under "OnManualTick": it unpacks dt from the params - the
// first entry, and only when its type tag is 2, zero otherwise - and
// calls VuDelayEntity_update @0x5dfc40, which is exactly
//
//   if ( pending ) { remaining -= dt;
//                    if ( remaining < 0.0f ) { pending = false; fire( "Out" ); } }
//
// STRICTLY less than zero, so a zero-length delay still costs one tick.
// Firing inside the sweep is retail's own order: a delay restarted by an
// Out still counts down this frame if it is later in the walk. The walk
// is ours though - retail's is the tick registration (load) order and
// ours is alphabetical by path - so two delays wired to each other can
// resolve a frame apart. FLAGGED, no constant involved.
inline void c_world::update_delays( float dt )
{
    for ( auto& pair : m_delays )
    {
        if ( !pair.second.pending )
            continue;
        pair.second.remaining -= dt;
        if ( pair.second.remaining >= 0.0f )
            continue;
        pair.second.pending = false;
        fire( pair.first, "Out" );
    }
}

inline void c_world::count_unhandled( const std::string& entity, const std::string& plug )
{
    if ( ++m_unhandled_events == 1 )
    {
        LOG_WARNING( "event %s . %s has no handler yet; further unhandled events are counted only",
            entity.c_str( ), plug.c_str( ) );
    }
}

// VuAnimatedPropEntity's VuAnimationEventIF slot 0 @0x5ed680 (the IDB
// calls it `..._load_EventName`, which is wrong). The control's walk calls
// it with each timed event's name and VUJB params, and with AnimDone /
// AnimLoop and no params when the clip ends or wraps.
inline void c_world::on_animation_event( const std::string& owner, const std::string& name, const value_t* params )
{
    // The first test compares min( length, 8 ) bytes against "AnimDone"
    // and then the lengths, so it is an exact match.
    if ( name == "AnimDone" )
    {
        fire( owner, "OnAnimDone" );
        return;
    }
    if ( name == "AnimLoop" )
    {
        fire( owner, "OnAnimLoop" );
        return;
    }
    if ( name == "BroadcastGenericEvent" )
    {
        // json "EventName", then the same global "OnGenericEvent" post as
        // VuGenericEventEntity's Broadcast @0x5f0330 - every instance whose
        // Event Name matches fires OnReceived. Retail posts only when the
        // name is non-empty (v26[4], the string's length).
        const value_t* named = params != nullptr ? find( *params, "EventName" ) : nullptr;
        const std::string carried = named != nullptr && named->kind == value_t::kind_t::text
            ? named->text : std::string( );
        if ( carried.empty( ) )
            return;
        for ( const auto& pair : m_events )
        {
            if ( pair.second == carried )
                fire( pair.first, "OnReceived" );
        }
        return;
    }
    // UNBUILT, named rather than guessed: PlayAudioEvent (FMOD event at
    // json Pos through the prop's matrix and scale), PlayPfxModelSpace
    // (json PfxName/Pos/Rot), Show and Hide (the prop's vtable slots 12
    // and 13, @0x5ecf90 / @0x5ecfe0, unread). Any other name does nothing
    // in retail either.
    if ( name == "PlayAudioEvent" || name == "PlayPfxModelSpace" || name == "Show" || name == "Hide" )
        count_unhandled( owner, name );
}

// Does an event sent to this plug reach the named prop? A side-effect-free
// mirror of dispatch(), for the wiring check below - the bench has no
// input, so the player never moves and never crosses a plane, which means
// a bench run alone can never exercise any of this.
inline bool c_world::reaches( const std::string& entity, const std::string& plug,
    const std::string& prop, int depth) const
{
    if ( depth > 8 )
        return false;
    if ( entity == prop )
        return true;
    const auto action = m_actions.find( entity );
    if ( action != m_actions.end( ) )
        return plug == "Trigger" && walks( entity, "PlayAnimation", prop, depth );
    const auto latch = m_filters.find( entity );
    if ( latch != m_filters.end( ) )
        return plug == "In" && walks( entity, "Out", prop, depth );
    if ( m_delays.count( entity ) != 0 )
        return plug == "In" && walks( entity, "Out", prop, depth );
    // An If takes one branch or the other, so either reaching the prop
    // makes it reachable.
    if ( m_ifs.count( entity ) != 0 )
        return plug == "Trigger" && ( walks( entity, "True", prop, depth )
            || walks( entity, "False", prop, depth ) );
    if ( m_booleans.count( entity ) != 0 )
    {
        if ( plug != "SetTrue" && plug != "SetFalse" && plug != "Toggle" )
            return false;
        return walks( entity, "OnChanged", prop, depth )
            || walks( entity, "OnChangedTrue", prop, depth )
            || walks( entity, "OnChangedFalse", prop, depth );
    }
    const auto event = m_events.find( entity );
    if ( event != m_events.end( ) && plug == "Broadcast" )
    {
        for ( const auto& pair : m_events )
        {
            if ( pair.second == event->second && walks( pair.first, "OnReceived", prop, depth ) )
                return true;
        }
    }
    return false;
}

inline bool c_world::animated_at( const std::string& path ) const
{
    for ( const animated_t& item : m_animated )
    {
        if ( item.path == path )
            return true;
    }
    return false;
}

// What the resolved graph actually holds, printed once per load. This is
// the measurement that says which plug names the unbuilt node kinds use -
// names that must be read, never guessed - and whether anything at all is
// wired to the entities we do handle. The nine point waves are called out
// by name because each one needs an "Emit" to arrive from somewhere.
inline void c_world::audit_graph( ) const
{
    std::map< std::string, int > waiting;
    for ( const auto& pair : m_graph )
    {
        for ( const edge_t& edge : pair.second )
        {
            const bool known = m_actions.count( edge.entity ) != 0
                || m_filters.count( edge.entity ) != 0
                || m_ripple_sources.count( edge.entity ) != 0
                || m_delays.count( edge.entity ) != 0
                || m_events.count( edge.entity ) != 0
                || m_ifs.count( edge.entity ) != 0
                || m_booleans.count( edge.entity ) != 0
                || animated_at( edge.entity );
            if ( known )
                continue;
            const auto type = m_entity_type.find( edge.entity );
            ++waiting[ ( type == m_entity_type.end( ) ? std::string( "?" ) : type->second )
                + " . " + edge.plug ];
        }
    }
    // Can anything actually reach each point wave? The bench never drives
    // far enough to cross the planes these hang off, so reachability is
    // the only check available without playing the course, and it is the
    // one that exercises the new node kinds end to end.
    int reachable = 0;
    for ( const auto& source : m_ripple_sources )
    {
        std::string via;
        for ( const trigger_t& gate : m_triggers.all( ) )
        {
            if ( walks( gate.path, "OnExit", source.first, 0 )
                || walks( gate.path, "OnEnter", source.first, 0 ) )
            {
                via = gate.path;
                break;
            }
        }
        for ( std::size_t i = 0; via.empty( ) && i < m_initialized.size( ); ++i )
        {
            if ( walks( m_initialized[ i ], "Trigger", source.first, 0 ) )
                via = m_initialized[ i ];
        }
        if ( !via.empty( ) )
        {
            ++reachable;
            LOG_SUCCESS( "point wave %s is reached from %s",
                source.first.c_str( ), via.c_str( ) );
            continue;
        }
        // Unreached: walk the chain BACKWARDS and print it, so the one
        // missing node kind is named instead of guessed at. Connections
        // are stored on both ends, so the reverse walk is the same map.
        LOG_WARNING( "point wave %s is reached from nothing built; upstream chain:",
            source.first.c_str( ) );
        std::set< std::pair< std::string, std::string > > seen;
        std::vector< std::pair< std::string, std::string > > front;
        front.emplace_back( source.first, "Emit" );
        for ( int depth = 0; depth < 6 && !front.empty( ); ++depth )
        {
            std::vector< std::pair< std::string, std::string > > next;
            for ( const auto& node : front )
            {
                for ( const auto& pair : m_graph )
                {
                    for ( const edge_t& edge : pair.second )
                    {
                        if ( edge.entity != node.first || edge.plug != node.second )
                            continue;
                        if ( !seen.insert( pair.first ).second )
                            continue;
                        const auto type = m_entity_type.find( pair.first.first );
                        LOG_WARNING( "   %*s%s . %s   [%s]", depth * 2, "",
                            pair.first.first.c_str( ), pair.first.second.c_str( ),
                            type == m_entity_type.end( ) ? "?" : type->second.c_str( ) );
                        next.push_back( pair.first );
                    }
                }
            }
            front.swap( next );
        }
    }
    LOG_INFO( "point waves: %d of %u reachable through the built node kinds",
        reachable, static_cast< unsigned >( m_ripple_sources.size( ) ) );
    // The refs, grouped by the owning class and ref name, so the census
    // says which constructors have to be read to consume them.
    std::map< std::string, int > by_ref;
    for ( const auto& pair : m_refs )
    {
        const auto type = m_entity_type.find( pair.first.first );
        ++by_ref[ ( type == m_entity_type.end( ) ? std::string( "?" ) : type->second )
            + " . " + pair.first.second ];
    }
    for ( const auto& pair : by_ref )
        LOG_INFO( "refs: %4d %s", pair.second, pair.first.c_str( ) );
    // THE ROTATOR AUDIT. update_rotators adopts the placements of the
    // subtree the motion entity SITS IN - an inference made before the
    // Refs block was parsed. VuMotionEntity_onActivate @0x61a4f0 drives
    // the "Entity" ref instead, so the two can now be compared: the
    // inference is sound exactly when the ref names an ANCESTOR of the
    // motion entity's own path.
    int absolute = 0, relative = 0, unresolved = 0, missing = 0;
    for ( const rotator_t& motion : m_rotators )
    {
        const auto ref = m_refs.find( std::make_pair( motion.path, "Entity" ) );
        if ( ref == m_refs.end( ) )
        {
            ++missing;
            LOG_WARNING( "rotator %s has no Entity ref", motion.path.c_str( ) );
            continue;
        }
        const std::string target = ref_target( motion.path, "Entity" );
        if ( m_entity_type.count( target ) == 0 )
        {
            ++unresolved;
            LOG_WARNING( "rotator %s: ref \"%s\" resolves to nothing",
                motion.path.c_str( ), ref->second.c_str( ) );
        }
        else if ( target == ref->second )
            ++absolute;
        else
            ++relative;
    }
    LOG_INFO( "rotator refs: %u rotators - %d absolute, %d template-relative, %d unresolved, %d without a ref",
        static_cast< unsigned >( m_rotators.size( ) ),
        absolute, relative, unresolved, missing );
    // THE CORONA CENSUS. The two draw branches we could not read out of
    // our own renderer - the screen-space sprite size and the roll's
    // screen-x term - only matter for coronas that author them, so say how
    // many do instead of leaving the substitution unmeasured. A corona
    // with no Texture Name has nothing to draw and is a parse failure, not
    // a feature gap.
    int screen_space = 0;
    int rolling = 0;
    int spinning = 0;
    int untextured = 0;
    int hidden = 0;
    for ( const c_corona_entity::corona_t& light : m_coronas.lights( ) )
    {
        if ( light.screen_space )
            ++screen_space;
        if ( light.rotation_amount != 0.0f )
            ++rolling;
        if ( light.rotation_speed != 0.0f )
            ++spinning;
        if ( light.texture.empty( ) )
            ++untextured;
        if ( !light.visible )
            ++hidden;
    }
    LOG_INFO( "coronas: %d placed - %d screen-space sized, %d roll with screen x, %d sweep a cone, %d start hidden, %d without a Texture Name",
        m_coronas.total( ), screen_space, rolling, spinning, hidden, untextured );
    // Where each one actually sits, with the two numbers that decide
    // whether it is ever on screen. A corona is only PROVEN once one is
    // seen, and the attach children cannot be in the right place until the
    // attach transform lands, so the positions are listed rather than
    // summarised.
    for ( const c_corona_entity::corona_t& light : m_coronas.lights( ) )
        LOG_INFO( "corona %s at %.0f %.0f %.0f - draw %.0f fade %.0f size %.1f cone %.0f deg tex %s",
            light.path.c_str( ), light.world[ 12 ], light.world[ 13 ], light.world[ 14 ],
            light.draw_distance, light.fade_distance, light.size,
            light.cone * 57.29578f, light.texture.c_str( ) );
    // WHAT THE ATTACH TRANSFORM ACTUALLY MOVED, and what it could not.
    // The composition is only as good as the parent matrix it reads, so
    // say how many parents were found, how many children landed somewhere
    // this engine can draw, and - the residual that matters - how many
    // parents are themselves driven by a rotate motion, because
    // m_seat_world holds the AUTHORED transform and a spinning parent
    // would carry its child round with it in retail.
    int joined = 0;
    int no_parent = 0;
    int undrawable = 0;
    int spun_parent = 0;
    for ( const attach_t& join : m_attaches )
    {
        if ( !join.attached )
            continue;
        const std::string parent = ref_target( join.path, "Parent" );
        const std::string child = ref_target( join.path, "Child" );
        if ( m_seat_world.count( parent ) == 0 )
        {
            ++no_parent;
            LOG_WARNING( "attach %s: Parent \"%s\" has no placed transform",
                join.path.c_str( ), parent.c_str( ) );
            continue;
        }
        for ( const rotator_t& motion : m_rotators )
            if ( ref_target( motion.path, "Entity" ) == parent )
                ++spun_parent;
        if ( m_coronas.owns( child ) )
        {
            ++joined;
            // Where the seat actually is. A child that lands on the origin
            // means the PARENT subtree carries no transform in the level
            // data, not that the composition is wrong. MEASURED for the
            // thirteen WarningSiren templates: the instance node's data
            // holds only ChildEntities, the template root is a bare
            // VuSceneEntity, and not one entity in the subtree - prop,
            // rotate motion, attach or corona - has a
            // VuTransformComponent. Those sirens have no authored position
            // at all, so something outside the scene file places them.
            const std::array< float, 16 >& seat = m_seat_world.at( parent );
            LOG_INFO( "attach %s: parent \"%s\" at %.0f %.0f %.0f, offset %.1f %.1f %.1f",
                join.path.c_str( ), parent.c_str( ),
                seat[ 12 ], seat[ 13 ], seat[ 14 ],
                join.position[ 0 ], join.position[ 1 ], join.position[ 2 ] );
        }
        else
            ++undrawable;
    }
    LOG_INFO( "attach transform: %d children seated on their parent, %d parents not placed, %d children this engine cannot move yet, %d parents also driven by a rotate motion",
        joined, no_parent, undrawable, spun_parent );
    LOG_INFO( "template overrides: %d children overridden by their instance, %d of them carrying the instance's transform",
        m_overrides_seen, m_overrides_placed );
    for ( const auto& pair : m_override_components )
        LOG_INFO( "  override component %-28s x%d (read from the template, not the instance)",
            pair.first.c_str( ), pair.second );
    // WHAT EACH ATTACH ACTUALLY JOINS. VuAttachEntity's own semantics are
    // read (see memory/audit.md), but whether an attach is VISIBLE here
    // depends on what the Child is: a model's placements can be driven,
    // an audio emitter or a pfx emitter cannot until those subsystems
    // exist. Printing both sides' classes is what decides the scope.
    std::map< std::string, int > joins;
    for ( const attach_t& join : m_attaches )
    {
        const std::string parent = ref_target( join.path, "Parent" );
        const std::string child = ref_target( join.path, "Child" );
        const auto parent_type = m_entity_type.find( parent );
        const auto child_type = m_entity_type.find( child );
        ++joins[ ( child_type == m_entity_type.end( ) ? std::string( "?" ) : child_type->second )
            + " -> " + ( parent_type == m_entity_type.end( ) ? std::string( "?" ) : parent_type->second )
            + ( join.node.empty( ) ? std::string( " (no node)" ) : std::string( " (node)" ) ) ];
    }
    for ( const auto& pair : joins )
        LOG_INFO( "attach: %4d %s", pair.second, pair.first.c_str( ) );
    // Every volume's own outgoing edges. These are the only entry points
    // the player can trip, so what they reach bounds what the graph can
    // ever do without a timed event or a global broadcast.
    for ( const trigger_t& gate : m_triggers.all( ) )
    {
        for ( const char* plug : { "OnEnter", "OnExit" } )
        {
            const auto found = m_graph.find( std::make_pair( gate.path, plug ) );
            if ( found == m_graph.end( ) )
                continue;
            for ( const edge_t& edge : found->second )
            {
                const auto type = m_entity_type.find( edge.entity );
                LOG_INFO( "%s %s . %s -> %s . %s [%s]", gate.box ? "box " : "plane",
                    gate.path.c_str( ), plug, edge.entity.c_str( ), edge.plug.c_str( ),
                    type == m_entity_type.end( ) ? "?" : type->second.c_str( ) );
            }
        }
    }
    for ( const auto& pair : waiting )
        LOG_INFO( "graph: %4d edges land on %s", pair.second, pair.first.c_str( ) );
}

// The side-effect-free mirror of fire().
inline bool c_world::walks( const std::string& owner, const std::string& plug,
    const std::string& prop, int depth ) const
{
    const auto found = m_graph.find( std::make_pair( owner, plug ) );
    if ( found == m_graph.end( ) )
        return false;
    for ( const edge_t& edge : found->second )
    {
        if ( reaches( edge.entity, edge.plug, prop, depth + 1 ) )
            return true;
    }
    return false;
}

// For each prop still waiting on a trigger, confirm some volume actually
// reaches it through the graph. A prop wired to nothing would sit at its
// bind pose forever and look merely static rather than broken.
//
// This is the wiring half of what used to be one test_triggers; the
// geometry half moved to c_trigger_set, which can check itself without
// knowing anything about props or the graph.
inline void c_world::test_wiring( ) const
{
    for ( const animated_t& item : m_animated )
    {
        if ( item.playing || item.clip.empty( ) )
            continue;
        bool wired = false;
        std::string via;
        for ( const trigger_t& gate : m_triggers.all( ) )
        {
            if ( !walks( gate.path, "OnExit", item.path, 0 )
                && !walks( gate.path, "OnEnter", item.path, 0 ) )
                continue;
            wired = true;
            via = gate.path;
            break;
        }
        // A prop that no plane reaches is not necessarily unwired: most of
        // them hang off the load-time event instead.
        for ( std::size_t i = 0; !wired && i < m_initialized.size( ); ++i )
        {
            if ( !walks( m_initialized[ i ], "Trigger", item.path, 0 ) )
                continue;
            wired = true;
            via = m_initialized[ i ] + " (project init)";
        }
        if ( wired )
        {
            LOG_SUCCESS( "trigger wiring: %-34s waits on %s",
                item.asset.c_str( ), via.c_str( ) );
        }
        else
        {
            LOG_WARNING( "trigger wiring: %-34s is reachable from nothing built (%s)",
                item.asset.c_str( ), item.trigger.c_str( ) );
        }
    }
}
