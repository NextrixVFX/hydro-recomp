#pragma once

#include <workspace/core/collision/epa.hxx>
#include <workspace/core/collision/gjk.hxx>

// gjkepa2_impl::EPA (Bullet 2.79), recreated from retail's disassembly
// (memory/audit.md "Batch 8e", THE EPA STRUCT). Retail's object is 0x2864
// bytes: +0 m_status (0 Valid .. 9 Failed), +4 m_result.c[4], +20
// m_result.p[4], +36 m_result.rank, +48 m_normal, +64 m_depth, +80
// m_sv_store[64], +0x850 m_fc_store[128] (64 b each), +0x2850 m_nextsv,
// +0x2854 m_hull, +0x285C m_stock. Pointers stay pointers here; the check
// maps them back to retail's addresses.
inline constexpr unsigned g_epa_max_vertices = 64;   // `cmp nextsv, 40h; jnb`
inline constexpr unsigned g_epa_max_faces = 128;     // Initialize's loop count
inline constexpr unsigned g_epa_max_iterations = 255; // `cmp it, 0FFh; jnb`
inline constexpr unsigned g_epa_i2m3[ 3 ] = { 2, 0, 1 }; // .rdata 0x6F6EAC (i1m3 is g_epa_imd3)

// sFace: n +0, d +16, p +20, c[3] +24, f[3] +36, l[2] +48 / +52, e[3] +56,
// pass +59 (bytes).
struct epa_face_t
{
    float n[ 4 ];
    float d;
    float p;
    epa_sv_t* c[ 3 ];
    epa_face_t* f[ 3 ];
    epa_face_t* l[ 2 ];
    uint8_t e[ 3 ];
    uint8_t pass;
};

struct epa_list_t
{
    epa_face_t* root;
    unsigned count;
};

struct epa_horizon_t
{
    epa_face_t* cf = nullptr;
    epa_face_t* ff = nullptr;
    unsigned nf = 0;
};

struct epa_t
{
    int status;
    epa_sv_t* c[ 4 ];
    float p[ 4 ];
    unsigned rank;
    float normal[ 4 ];
    float depth;
    epa_sv_t sv_store[ g_epa_max_vertices ];
    epa_face_t fc_store[ g_epa_max_faces ];
    unsigned nextsv;
    epa_list_t hull;
    epa_list_t stock;
};

// append, inlined everywhere: l[0] = 0, l[1] = root, root.l[0] = face.
inline void epa_list_append( epa_list_t& list, epa_face_t* face )
{
    face->l[ 0 ] = nullptr;
    face->l[ 1 ] = list.root;
    if ( list.root )
        list.root->l[ 0 ] = face;
    list.root = face;
    ++list.count;
}

// remove @0x610FF0 (fastcall ecx list, edx face; also inlined). The face's
// own links are left as they were.
inline void epa_list_remove( epa_list_t& list, epa_face_t* face )
{
    if ( face->l[ 1 ] )
        face->l[ 1 ]->l[ 0 ] = face->l[ 0 ];
    if ( face->l[ 0 ] )
        face->l[ 0 ]->l[ 1 ] = face->l[ 1 ];
    if ( face == list.root )
        list.root = face->l[ 1 ];
    --list.count;
}

// EPA::Initialize @0x611020 (thiscall): status Failed, normal (16 bytes) and
// depth zero, nextsv 0, fc_store[127..0] onto the stock. The hull is NOT
// reset.
inline void epa_init( epa_t& e )
{
    e.status = 9;
    e.normal[ 0 ] = e.normal[ 1 ] = e.normal[ 2 ] = e.normal[ 3 ] = 0.0f;
    e.depth = 0.0f;
    e.nextsv = 0;
    for ( unsigned i = 0; i < g_epa_max_faces; ++i )
        epa_list_append( e.stock, &e.fc_store[ g_epa_max_faces - i - 1 ] );
}

// findbest @0x611D30: a face replaces the best only when `f.p >= best.p`
// (comiss + jb: unordered skips) and `best.d^2 > f.d^2` (comiss + jbe).
inline epa_face_t* epa_findbest( epa_t& e )
{
    epa_face_t* minf = e.hull.root;
    float mind = minf->d * minf->d;
    float maxp = minf->p;
    for ( epa_face_t* f = minf->l[ 1 ]; f; f = f->l[ 1 ] )
    {
        const float sqd = f->d * f->d;
        if ( f->p >= maxp && mind > sqd )
        {
            minf = f;
            mind = sqd;
            maxp = f->p;
        }
    }
    return minf;
}

// P . cross(n, P - Q), retail's term order: cross = (n.y e.z - n.z e.y,
// n.z e.x - n.x e.z, n.x e.y - n.y e.x), dot = (P.y c.y + P.x c.x) + P.z c.z.
inline float epa_edge_dot( const float* n, const float* pt, const float* q )
{
    const float ex = pt[ 0 ] - q[ 0 ], ey = pt[ 1 ] - q[ 1 ], ez = pt[ 2 ] - q[ 2 ];
    const float cx = n[ 1 ] * ez - n[ 2 ] * ey;
    const float cy = n[ 2 ] * ex - n[ 0 ] * ez;
    const float cz = n[ 0 ] * ey - n[ 1 ] * ex;
    return ( pt[ 1 ] * cy + pt[ 0 ] * cx ) + pt[ 2 ] * cz;
}

// newface @0x6118D0 (thiscall; a, b, c, forced; retn 10h). No stock face:
// status OutOfFaces (5). n = (b - a) x (c - a) written term for term; v =
// ordered |n| > 1e-4 (0x6F75A4). p = min of the three edge dots - c's
// first, then b's, then a's (cmovbe, cmova) - over (v ? |n| : 1), then 0 if
// ordered p >= -0.01 (0x6F787C). v: r = 1 / |n|; d = ((n.x a.x + n.y a.y)
// + a.z n.z) r BEFORE n *= r; kept if forced or ordered d >= -1e-5
// (0x6F7874), else NonConvex (3). !v: Degenerated (2). A rejected face goes
// back from the hull to the stock.
inline epa_face_t* epa_newface( epa_t& e, epa_sv_t* a, epa_sv_t* b, epa_sv_t* c, bool forced )
{
    epa_face_t* face = e.stock.root;
    if ( !face )
    {
        e.status = 5;
        return nullptr;
    }
    epa_list_remove( e.stock, face );
    epa_list_append( e.hull, face );
    face->pass = 0;
    face->c[ 0 ] = a;
    face->c[ 1 ] = b;
    face->c[ 2 ] = c;
    const float abx = b->w[ 0 ] - a->w[ 0 ], aby = b->w[ 1 ] - a->w[ 1 ], abz = b->w[ 2 ] - a->w[ 2 ];
    const float acx = c->w[ 0 ] - a->w[ 0 ], acy = c->w[ 1 ] - a->w[ 1 ], acz = c->w[ 2 ] - a->w[ 2 ];
    face->n[ 0 ] = aby * acz - abz * acy;
    face->n[ 1 ] = abz * acx - abx * acz;
    face->n[ 2 ] = abx * acy - aby * acx;
    face->n[ 3 ] = 0.0f;
    const float* n = face->n;
    const float l = std::sqrt( ( n[ 0 ] * n[ 0 ] + n[ 1 ] * n[ 1 ] ) + n[ 2 ] * n[ 2 ] );
    const bool v = l > 1e-4f;
    const float dc = epa_edge_dot( n, c->w, a->w );
    const float db = epa_edge_dot( n, b->w, c->w );
    const float da = epa_edge_dot( n, a->w, b->w );
    const float m1 = db > da ? da : db;
    const float m = dc > m1 ? m1 : dc;
    face->p = m / ( v ? l : 1.0f );
    if ( face->p >= -0.01f )
        face->p = 0.0f;
    if ( v )
    {
        const float r = 1.0f / l;
        face->d = ( ( face->n[ 0 ] * a->w[ 0 ] + face->n[ 1 ] * a->w[ 1 ] ) + a->w[ 2 ] * face->n[ 2 ] ) * r;
        face->n[ 0 ] = face->n[ 0 ] * r;
        face->n[ 1 ] = face->n[ 1 ] * r;
        face->n[ 2 ] = face->n[ 2 ] * r;
        if ( forced || face->d >= -1e-5f )
            return face;
        e.status = 3;
    }
    else
        e.status = 2;
    epa_list_remove( e.hull, face );
    epa_list_append( e.stock, face );
    return nullptr;
}

// expand @0x611D80 (thiscall; pass, w, f, e, horizon; retn 14h). The byte
// pass is compared zero-extended against the full 32-bit pass. Visible when
// ordered -1e-5 > ((w.y n.y + n.x w.x) + w.z n.z) - d: a new face (f.c[e1],
// f.c[e], w) bound to f across e and chained to the horizon (binds inlined);
// otherwise f is marked, both other edges recurse, and f moves to the stock.
inline bool epa_expand( epa_t& e, unsigned pass, epa_sv_t* w, epa_face_t* f, unsigned edge, epa_horizon_t& horizon )
{
    if ( f->pass == pass )
        return false;
    const unsigned e1 = g_epa_imd3[ edge ];
    const float dist = ( ( w->w[ 1 ] * f->n[ 1 ] + f->n[ 0 ] * w->w[ 0 ] ) + w->w[ 2 ] * f->n[ 2 ] ) - f->d;
    if ( -1e-5f > dist )
    {
        epa_face_t* nf = epa_newface( e, f->c[ e1 ], f->c[ edge ], w, false );
        if ( !nf )
            return false;
        nf->e[ 0 ] = static_cast< uint8_t >( edge );
        nf->f[ 0 ] = f;
        f->e[ edge ] = 0;
        f->f[ edge ] = nf;
        if ( horizon.cf )
        {
            horizon.cf->f[ 1 ] = nf;
            horizon.cf->e[ 1 ] = 2;
            nf->e[ 2 ] = 1;
            nf->f[ 2 ] = horizon.cf;
            ++horizon.nf;
        }
        else
        {
            ++horizon.nf;
            horizon.ff = nf;
        }
        horizon.cf = nf;
        return true;
    }
    const unsigned e2 = g_epa_i2m3[ edge ];
    f->pass = static_cast< uint8_t >( pass );
    if ( epa_expand( e, pass, w, f->f[ e1 ], f->e[ e1 ], horizon ) && epa_expand( e, pass, w, f->f[ e2 ], f->e[ e2 ], horizon ) )
    {
        epa_list_remove( e.hull, f );
        epa_list_append( e.stock, f );
        return true;
    }
    return false;
}

// bind, inlined everywhere: fa.e[ea] = eb, fa.f[ea] = fb and back.
inline void epa_bind( epa_face_t* fa, unsigned ea, epa_face_t* fb, unsigned eb )
{
    fa->e[ ea ] = static_cast< uint8_t >( eb );
    fa->f[ ea ] = fb;
    fb->e[ eb ] = static_cast< uint8_t >( ea );
    fb->f[ eb ] = fa;
}

// |(a - q) x (b - q)| as the result tail writes it: x = u.y v.z - u.z v.y,
// y = u.z v.x - u.x v.z, z = u.x v.y - u.y v.x, sqrt((y^2 + x^2) + z^2).
inline float epa_area( const float* a, const float* b, const float* q )
{
    const float ux = a[ 0 ] - q[ 0 ], uy = a[ 1 ] - q[ 1 ], uz = a[ 2 ] - q[ 2 ];
    const float vx = b[ 0 ] - q[ 0 ], vy = b[ 1 ] - q[ 1 ], vz = b[ 2 ] - q[ 2 ];
    const float cx = uy * vz - uz * vy;
    const float cy = uz * vx - ux * vz;
    const float cz = ux * vy - uy * vx;
    return std::sqrt( ( cy * cy + cx * cx ) + cz * cz );
}

// EPA::Evaluate @0x6110A0 (thiscall epa; gjk*, guess*; retn 8), from the
// disassembly. rank > 1 (unsigned `jbe`) and EncloseOrigin, else the
// fallback. The hull goes back to the stock; status 0, nextsv 0; the simplex
// is reoriented (c0 / c1 and p0 / p1 swapped) when ordered 0 > det(c0 - c3,
// c1 - c3, c2 - c3); four forced faces (c0 c1 c2), (c1 c0 c3), (c2 c1 c3),
// (c0 c2 c3); a hull of any other count than 4 falls back. Each iteration:
// OutOfVertices (6) at nextsv >= 64; best.pass = ++pass; the support along
// best.n; AccuraryReached (7) unless ordered ((w.y n.y + w.x n.x) + w.z n.z)
// - d > 1e-4; expand over the three edges, skipping the rest once one
// fails; InvalidHull (4) on a failure or fewer than three new faces; outer
// takes the new best on ordered `best.p >= outer.p` (comiss + jb). 255
// iterations at most (status left Valid). The result: normal (four lanes) /
// depth / c[0..2] from outer, rank 3, p[] the three areas (c1 c2), (c2 c0),
// (c0 c1) about n d, scaled by ONE reciprocal 1 / ((p1 + p0) + p2) - retail
// multiplies where the 2.79 source divides. The fallback: FallBack (8),
// normal = -guess (sign flips, w 0) times 1 / |.| when ordered |.| > 0, else
// (1, 0, 0, 0); depth 0, rank 1, c[0] = the simplex's c[0], p[0] = 1. Our
// GJK keeps store indices: a face's vertex is &gjk.store[k]. Returns -1
// where a support reaches code not yet recreated.
inline int epa_evaluate( epa_t& e, epa_gjk_t& gjk, const float* guess )
{
    epa_simplex_t& s = gjk.simplices[ gjk.simplex ];
    bool enclosed = false;
    if ( s.rank > 1 )
    {
        const int r = epa_enclose_origin( gjk );
        if ( r < 0 )
            return -1;
        enclosed = r != 0;
    }
    if ( enclosed )
    {
        while ( e.hull.root )
        {
            epa_face_t* f = e.hull.root;
            epa_list_remove( e.hull, f );
            epa_list_append( e.stock, f );
        }
        e.status = 0;
        e.nextsv = 0;
        auto sv = [ & ]( int k ) { return &gjk.store[ s.c[ k ] ]; };
        float a[ 3 ], b[ 3 ], c[ 3 ];
        for ( int k = 0; k < 3; ++k )
        {
            a[ k ] = sv( 0 )->w[ k ] - sv( 3 )->w[ k ];
            b[ k ] = sv( 1 )->w[ k ] - sv( 3 )->w[ k ];
            c[ k ] = sv( 2 )->w[ k ] - sv( 3 )->w[ k ];
        }
        if ( 0.0f > epa_det( a, b, c ) )
        {
            const int c0 = s.c[ 0 ];
            s.c[ 0 ] = s.c[ 1 ];
            s.c[ 1 ] = c0;
            const float p0 = s.p[ 0 ];
            s.p[ 0 ] = s.p[ 1 ];
            s.p[ 1 ] = p0;
        }
        epa_face_t* tetra[ 4 ] = {
            epa_newface( e, sv( 0 ), sv( 1 ), sv( 2 ), true ),
            epa_newface( e, sv( 1 ), sv( 0 ), sv( 3 ), true ),
            epa_newface( e, sv( 2 ), sv( 1 ), sv( 3 ), true ),
            epa_newface( e, sv( 0 ), sv( 2 ), sv( 3 ), true ),
        };
        if ( e.hull.count == 4 )
        {
            epa_face_t* best = epa_findbest( e );
            epa_face_t outer = *best;
            unsigned pass = 0, iterations = 0;
            epa_bind( tetra[ 0 ], 0, tetra[ 1 ], 0 );
            epa_bind( tetra[ 0 ], 1, tetra[ 2 ], 0 );
            epa_bind( tetra[ 0 ], 2, tetra[ 3 ], 0 );
            epa_bind( tetra[ 1 ], 1, tetra[ 3 ], 2 );
            epa_bind( tetra[ 1 ], 2, tetra[ 2 ], 1 );
            epa_bind( tetra[ 2 ], 2, tetra[ 3 ], 1 );
            e.status = 0;
            for ( ;; )
            {
                if ( e.nextsv >= g_epa_max_vertices )
                {
                    e.status = 6;
                    break;
                }
                epa_horizon_t horizon;
                epa_sv_t* w = &e.sv_store[ e.nextsv++ ];
                bool valid = true;
                best->pass = static_cast< uint8_t >( ++pass );
                if ( !epa_get_support( gjk.shape, best->n, *w ) )
                    return -1;
                const float wdist = ( ( w->w[ 1 ] * best->n[ 1 ] + w->w[ 0 ] * best->n[ 0 ] ) + w->w[ 2 ] * best->n[ 2 ] ) - best->d;
                if ( !( wdist > 1e-4f ) )
                {
                    e.status = 7;
                    break;
                }
                for ( unsigned j = 0; j < 3 && valid; ++j )
                    valid = epa_expand( e, pass, w, best->f[ j ], best->e[ j ], horizon ) && valid;
                if ( !valid || horizon.nf < 3 )
                {
                    e.status = 4;
                    break;
                }
                epa_bind( horizon.cf, 1, horizon.ff, 2 );
                epa_list_remove( e.hull, best );
                epa_list_append( e.stock, best );
                best = epa_findbest( e );
                if ( best->p >= outer.p )
                    outer = *best;
                if ( ++iterations >= g_epa_max_iterations )
                    break;
            }
            std::memcpy( e.normal, outer.n, 16 );
            e.depth = outer.d;
            e.rank = 3;
            e.c[ 0 ] = outer.c[ 0 ];
            e.c[ 1 ] = outer.c[ 1 ];
            e.c[ 2 ] = outer.c[ 2 ];
            const float q[ 3 ] = { outer.n[ 0 ] * outer.d, outer.n[ 1 ] * outer.d, outer.n[ 2 ] * outer.d };
            const float p0 = epa_area( outer.c[ 1 ]->w, outer.c[ 2 ]->w, q );
            const float p1 = epa_area( outer.c[ 2 ]->w, outer.c[ 0 ]->w, q );
            const float p2 = epa_area( outer.c[ 0 ]->w, outer.c[ 1 ]->w, q );
            const float r = 1.0f / ( ( p1 + p0 ) + p2 );
            e.p[ 0 ] = r * p0;
            e.p[ 1 ] = r * p1;
            e.p[ 2 ] = r * p2;
            return e.status;
        }
    }
    e.status = 8;
    float n[ 4 ] = { epa_negate( guess[ 0 ] ), epa_negate( guess[ 1 ] ), epa_negate( guess[ 2 ] ), 0.0f };
    const float nl = std::sqrt( ( n[ 0 ] * n[ 0 ] + n[ 1 ] * n[ 1 ] ) + n[ 2 ] * n[ 2 ] );
    if ( nl > 0.0f )
    {
        const float r = 1.0f / nl;
        n[ 0 ] = n[ 0 ] * r;
        n[ 1 ] = n[ 1 ] * r;
        n[ 2 ] = n[ 2 ] * r;
    }
    else
    {
        n[ 0 ] = 1.0f;
        n[ 1 ] = n[ 2 ] = 0.0f;
    }
    std::memcpy( e.normal, n, 16 );
    e.depth = 0.0f;
    e.rank = 1;
    e.c[ 0 ] = &gjk.store[ s.c[ 0 ] ];
    e.p[ 0 ] = 1.0f;
    return e.status;
}

// wtrs * v as Penetration writes it: x = ((v.y m[1] + v.x m[0]) + v.z m[2])
// + o.x, y likewise with row 1, z = ((v.x m[8] + v.y m[9]) + v.z m[10]) +
// o.z; w 0.
inline void epa_to_world( const float* m, const float* v, float* out )
{
    out[ 0 ] = ( ( v[ 1 ] * m[ 1 ] + v[ 0 ] * m[ 0 ] ) + v[ 2 ] * m[ 2 ] ) + m[ 12 ];
    out[ 1 ] = ( ( v[ 1 ] * m[ 5 ] + m[ 4 ] * v[ 0 ] ) + v[ 2 ] * m[ 6 ] ) + m[ 13 ];
    out[ 2 ] = ( ( v[ 0 ] * m[ 8 ] + v[ 1 ] * m[ 9 ] ) + v[ 2 ] * m[ 10 ] ) + m[ 14 ];
    out[ 3 ] = 0.0f;
}

// btGjkEpaSolver2::Penetration @0x6129E0 (ecx shape0, edx wtrs0; shape1,
// wtrs1, guess, results on the stack; the caller pops; al). MinkowskiDiff
// Initialize with margins; the GJK's inline ctor (ray, distance, nfree,
// current 0, status Failed); GJK::Evaluate along -guess (sign flips, w 0):
// Inside goes on, Failed sets results.status 2, anything else returns false.
// The EPA's inline ctor zeroes hull and stock; EPA::Initialize, then
// EPA::Evaluate with the same -guess; Failed (9) sets results.status 3 and
// returns false (EPA::Evaluate itself only leaves 0..8). Otherwise w0 = sum
// of shape 0's support along c[i]->d times p[i] (each `s p + w0`), status
// Penetrating (1), witness 0 = wtrs0 w0, witness 1 = wtrs0 (w0 - n depth),
// normal = -n (w 0), distance = -depth, true. Returns 1 / 0, or -1 where a
// support reaches code not yet recreated.
inline int epa_penetration( const convex_shape_t* shape0, const float* wtrs0, const convex_shape_t* shape1, const float* wtrs1,
    const float* guess, epa_results_t& results )
{
    epa_shape_t shape{};
    epa_initialize( shape0, wtrs0, shape1, wtrs1, results, shape, true );
    epa_gjk_t gjk;
    gjk.status = 2;
    const float g[ 4 ] = { epa_negate( guess[ 0 ] ), epa_negate( guess[ 1 ] ), epa_negate( guess[ 2 ] ), 0.0f };
    if ( !epa_gjk_evaluate( gjk, shape, g ) )
        return -1;
    if ( gjk.status != 1 )
    {
        if ( gjk.status == 2 )
            results.status = 2;
        return 0;
    }
    static epa_t e;
    std::memset( &e, 0, sizeof( e ) );
    epa_init( e );
    const int status = epa_evaluate( e, gjk, g );
    if ( status < 0 )
        return -1;
    if ( status == 9 )
    {
        results.status = 3;
        return 0;
    }
    float w0[ 3 ] = { 0.0f, 0.0f, 0.0f };
    for ( unsigned i = 0; i < e.rank; ++i )
    {
        float s[ 4 ];
        if ( !epa_shape_support( shape, 0, e.c[ i ]->d, s ) )
            return -1;
        w0[ 0 ] = s[ 0 ] * e.p[ i ] + w0[ 0 ];
        w0[ 1 ] = s[ 1 ] * e.p[ i ] + w0[ 1 ];
        w0[ 2 ] = s[ 2 ] * e.p[ i ] + w0[ 2 ];
    }
    results.status = 1;
    epa_to_world( wtrs0, w0, results.witness[ 0 ] );
    const float v[ 3 ] = { w0[ 0 ] - e.normal[ 0 ] * e.depth, w0[ 1 ] - e.normal[ 1 ] * e.depth, w0[ 2 ] - e.normal[ 2 ] * e.depth };
    epa_to_world( wtrs0, v, results.witness[ 1 ] );
    results.normal[ 0 ] = epa_negate( e.normal[ 0 ] );
    results.normal[ 1 ] = epa_negate( e.normal[ 1 ] );
    results.normal[ 2 ] = epa_negate( e.normal[ 2 ] );
    results.normal[ 3 ] = 0.0f;
    results.distance = epa_negate( e.depth );
    return 1;
}

// MinkowskiDiff::Support1 as Distance inlines it: n = -d (sign flips),
// shape 1's support along m_toshape1 n, through m_toshape0 - the same terms
// as getsupport's second half (epa_get_support).
inline bool epa_support1( const epa_shape_t& shape, const float* d, float* out )
{
    const float n[ 3 ] = { epa_negate( d[ 0 ] ), epa_negate( d[ 1 ] ), epa_negate( d[ 2 ] ) };
    const float* t1 = shape.toshape1;
    float dir1[ 4 ];
    dir1[ 0 ] = ( t1[ 1 ] * n[ 1 ] + t1[ 0 ] * n[ 0 ] ) + t1[ 2 ] * n[ 2 ];
    dir1[ 1 ] = ( t1[ 5 ] * n[ 1 ] + t1[ 4 ] * n[ 0 ] ) + t1[ 6 ] * n[ 2 ];
    dir1[ 2 ] = ( t1[ 9 ] * n[ 1 ] + t1[ 8 ] * n[ 0 ] ) + t1[ 10 ] * n[ 2 ];
    dir1[ 3 ] = 0.0f;
    float s1[ 4 ];
    if ( !epa_shape_support( shape, 1, dir1, s1 ) )
        return false;
    const float* t0 = shape.toshape0;
    for ( int r0 = 0; r0 < 3; ++r0 )
        out[ r0 ] = ( ( t0[ 4 * r0 + 1 ] * s1[ 1 ] + s1[ 0 ] * t0[ 4 * r0 ] ) + t0[ 4 * r0 + 2 ] * s1[ 2 ] ) + t0[ 12 + r0 ];
    return true;
}

// btGjkEpaSolver2::Distance @0x612410 (ecx shape0, edx wtrs0; shape1, wtrs1,
// guess, results; the caller pops; al). Initialize WITHOUT margins; the
// GJK's inline ctor; GJK::Evaluate along the guess AS GIVEN. Valid: w0 / w1
// = sums of Support0(c[i]->d) and Support1(-c[i]->d) times p[i] (each `s p
// + w`); witnesses wtrs0 w0 / wtrs0 w1; normal = w0 - w1 (w 0); distance =
// |normal| (stored unscaled); normal *= 1 / distance when ordered distance
// > 1e-4 (0x6F75A4), else * 1/1; status left Separated (0); true.
// Otherwise status 1 for Inside, 2 for anything else; false. Returns 1 /
// 0, or -1 where a support reaches code not yet recreated.
inline int epa_distance( const convex_shape_t* shape0, const float* wtrs0, const convex_shape_t* shape1, const float* wtrs1,
    const float* guess, epa_results_t& results )
{
    epa_shape_t shape{};
    epa_initialize( shape0, wtrs0, shape1, wtrs1, results, shape, false );
    epa_gjk_t gjk;
    gjk.status = 2;
    if ( !epa_gjk_evaluate( gjk, shape, guess ) )
        return -1;
    if ( gjk.status != 0 )
    {
        results.status = gjk.status == 1 ? 1 : 2;
        return 0;
    }
    const epa_simplex_t& s = gjk.simplices[ gjk.simplex ];
    float w0[ 3 ] = { 0.0f, 0.0f, 0.0f }, w1[ 3 ] = { 0.0f, 0.0f, 0.0f };
    for ( unsigned i = 0; i < s.rank; ++i )
    {
        const float p = s.p[ i ];
        const float* d = gjk.store[ s.c[ i ] ].d;
        float s0[ 4 ], s1[ 3 ];
        if ( !epa_shape_support( shape, 0, d, s0 ) )
            return -1;
        w0[ 0 ] = s0[ 0 ] * p + w0[ 0 ];
        w0[ 1 ] = s0[ 1 ] * p + w0[ 1 ];
        w0[ 2 ] = s0[ 2 ] * p + w0[ 2 ];
        if ( !epa_support1( shape, d, s1 ) )
            return -1;
        w1[ 0 ] = s1[ 0 ] * p + w1[ 0 ];
        w1[ 1 ] = s1[ 1 ] * p + w1[ 1 ];
        w1[ 2 ] = s1[ 2 ] * p + w1[ 2 ];
    }
    epa_to_world( wtrs0, w0, results.witness[ 0 ] );
    epa_to_world( wtrs0, w1, results.witness[ 1 ] );
    results.normal[ 0 ] = w0[ 0 ] - w1[ 0 ];
    results.normal[ 1 ] = w0[ 1 ] - w1[ 1 ];
    results.normal[ 2 ] = w0[ 2 ] - w1[ 2 ];
    results.normal[ 3 ] = 0.0f;
    const float* n = results.normal;
    results.distance = std::sqrt( ( n[ 0 ] * n[ 0 ] + n[ 1 ] * n[ 1 ] ) + n[ 2 ] * n[ 2 ] );
    const float r = 1.0f / ( results.distance > 1e-4f ? results.distance : 1.0f );
    results.normal[ 0 ] = r * results.normal[ 0 ];
    results.normal[ 1 ] = r * results.normal[ 1 ];
    results.normal[ 2 ] = r * results.normal[ 2 ];
    return 1;
}

// btGjkEpaPenetrationDepthSolver::calcPenDepth @0x5BAF60 (thiscall, ten
// stack args, retn 28h): `this`, the simplex solver, the debug drawer and
// the stack allocator are never read. guess = oA - oB (w 0); Penetration -
// true: pa = witness 0, pb = witness 1, v = normal (16 bytes each), 1;
// false: Distance, and on its true the same three copies - but 0 either way.
// Returns 1 / 0, or -1 where a support reaches code not yet recreated.
inline int epa_calc_pen_depth( const convex_shape_t& a, const convex_shape_t& b, const float* transform_a, const float* transform_b,
    float* v, float* pa, float* pb )
{
    const float guess[ 4 ] = { transform_a[ 12 ] - transform_b[ 12 ], transform_a[ 13 ] - transform_b[ 13 ],
        transform_a[ 14 ] - transform_b[ 14 ], 0.0f };
    epa_results_t results;
    const int pen = epa_penetration( &a, transform_a, &b, transform_b, guess, results );
    if ( pen < 0 )
        return -1;
    int ret = 1;
    if ( pen == 0 )
    {
        ret = 0;
        const int dist = epa_distance( &a, transform_a, &b, transform_b, guess, results );
        if ( dist < 0 )
            return -1;
        if ( dist == 0 )
            return 0;
    }
    std::memcpy( pa, results.witness[ 0 ], 16 );
    std::memcpy( pb, results.witness[ 1 ], 16 );
    std::memcpy( v, results.normal, 16 );
    return ret;
}

// gjk_hooks_t::pen_depth for a detector whose pd solver is a
// btGjkEpaPenetrationDepthSolver (vt 0x74E8B0, slot +4 = calcPenDepth): the
// simplex, debug drawer and stack allocator go unread, as in retail.
inline int epa_pen_depth_hook( void*, simplex_solver_t&, const convex_shape_t& a, const convex_shape_t& b, const float* transform_a,
    const float* transform_b, float* v, float* pa, float* pb, void*, void* )
{
    return epa_calc_pen_depth( a, b, transform_a, transform_b, v, pa, pb );
}
