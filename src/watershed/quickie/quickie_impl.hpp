//
// Copyright (C) 2010  Aleksandar Zlateski <zlateski@mit.edu>
// ----------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef ZI_WATERSHED_QUICKIE_QUICKIE_IMPL_HPP
#define ZI_WATERSHED_QUICKIE_QUICKIE_IMPL_HPP 1

#include <detail/types.hpp>
#include <detail/traits.hpp>
#include <detail/utility.hpp>
#include <detail/volume_ref.hpp>
#include <detail/buffer.hpp>
#include <detail/tictoc.hpp>

#include <zi/bits/cstdint.hpp>
#include <zi/bits/tuple.hpp>
#include <zi/bits/function.hpp>
#include <zi/bits/type_traits.hpp>
#include <zi/bits/unordered_map.hpp>

#include <zi/utility/for_each.hpp>
#include <zi/utility/assert.hpp>
#include <zi/utility/static_assert.hpp>

#include <zi/parallel/algorithm.hpp>

#include <zi/disjoint_sets/disjoint_sets.hpp>

#include <vector>
#include <queue>
#include <deque>

#include <functional>
#include <algorithm>
#include <utility>
#include <cstddef>
#include <iostream>
#include <limits>

#include <quickie/prefix.hpp>

#if defined( NDEBUG  ) || defined ( ZI_NO_DEBUG  )
#  define ZI_IF_DEBUG( ...  ) static_cast< void >( 0  )
#else
#  define ZI_IF_DEBUG( ...  ) __VA_ARGS__
#endif

namespace zi {
namespace watershed {

namespace detail {

template <typename T, typename I>
struct affinity_edge_sorter
{
private:
    const T* d_;

public:
    affinity_edge_sorter( const T* d = 0 ): d_(d) {}

    bool operator()(const I& lhs, const I& rhs) const
    {
        ZI_ASSERT(d_!=0);
        return d_[lhs]>d_[rhs];
    }
};

} // namespace detail

struct border
{
    static const int none     = 0x00;
    static const int before_x = 0x01;
    static const int after_x  = 0x08;
    static const int before_y = 0x02;
    static const int after_y  = 0x10;
    static const int before_z = 0x04;
    static const int after_z  = 0x20;
    static const int all      = 0x3f;
};

template< class ValueType, class SegmentIdType, class SizeType >
inline size_type
chunk_quickie_impl( const size_type      xsize,
                    const size_type      ysize,
                    const size_type      zsize,
                    const ValueType*     conn,
                    const ValueType      high,
                    const ValueType      low,
                    const SizeType       merge_size,
                    const ValueType      merge_low,
                    const SizeType       dust_size,
                    const int            borders,
                    SegmentIdType*       seg,
                    std::vector< SizeType >& counts,
                    std::vector< tuple< SegmentIdType, SegmentIdType, ValueType > >& dendr )
{
    ZI_STATIC_ASSERT(!is_const<SegmentIdType>::type::value, should_be_constant);
    ZI_STATIC_ASSERT(is_integral<SegmentIdType>::type::value, should_be_integer);
    ZI_STATIC_ASSERT(is_integral<SizeType>::type::value, should_be_integer);
    ZI_STATIC_ASSERT(is_unsigned<SizeType>::type::value, should_be_unsigned);
    ZI_STATIC_ASSERT(is_arithmetic<ValueType>::type::value, should_be_arithmetic);

    typedef ValueType                         value_type ;
    typedef SizeType                          count_type ;
    typedef SegmentIdType                     id_type    ;

    typedef std::pair< id_type, id_type >          id_pair;
    typedef tuple< id_type, id_type, value_type >  edge_type;

    ZI_ASSERT(xsize*ysize*zsize<(~(1ul<<(std::numeric_limits<count_type>::digits-1))));
    ZI_ASSERT(xsize*ysize*zsize<(~(1ul<<(std::numeric_limits<id_type>::digits-1))));

    static const id_type    assigned_bit = (1ul<<(std::numeric_limits<id_type>::digits-1));
    static const id_type    visited_bit  = 0x40;
    static const count_type on_border    = (1ul<<(std::numeric_limits<count_type>::digits-1));


    const size_type size = xsize*ysize*zsize;
    const index     xdim = static_cast< index >( xsize );
    const index     ydim = static_cast< index >( ysize );
    const index     zdim = static_cast< index >( zsize );

    //
    // Initialization
    //

    std::fill_n( seg, size, 0 );
    counts.clear();
    dendr.clear();

    //
    // make a graph representation
    //
    const_volume_ref< value_type, 4 > cmat( conn, extents[ xdim ][ ydim ][ zdim ][ 3 ] );
    volume_ref< id_type, 3 >          ids ( seg,  extents[ xdim ][ ydim ][ zdim ] );

    const id_type front_x_border = borders & border::before_x ? 0x0 : 0x08;
    const id_type back_x_border  = borders & border::after_x  ? 0x0 : 0x01;
    const id_type front_y_border = borders & border::before_y ? 0x0 : 0x10;
    const id_type back_y_border  = borders & border::after_y  ? 0x0 : 0x02;
    const id_type front_z_border = borders & border::before_z ? 0x0 : 0x20;
    const id_type back_z_border  = borders & border::after_z  ? 0x0 : 0x04;

    //tic("QuickieWs Start");

    QW_FOR_2( index, z, 1, zdim-1, y, 1, ydim-1 )
    {
        ids[0][y][z]      = cmat[1][y][z][0]      >= high ? 0x08 : front_x_border;
        ids[xdim-1][y][z] = cmat[xdim-1][y][z][0] >= high ? 0x01 : back_x_border ;
    }

    QW_FOR_2( index, z, 1, zdim-1, x, 1, xdim-1 )
    {
        ids[x][0][z]      = cmat[x][1][z][1]      >= high ? 0x10 : front_y_border;
        ids[x][ydim-1][z] = cmat[x][ydim-1][z][1] >= high ? 0x02 : back_y_border ;
    }

    QW_FOR_2( index, y, 1, ydim-1, x, 1, xdim-1 )
    {
        ids[x][y][0]      = cmat[x][y][1][2]      >= high ? 0x20 : front_z_border;
        ids[x][y][zdim-1] = cmat[x][y][zdim-1][2] >= high ? 0x04 : back_z_border ;
    }

    QW_FOR_3( index, z, 1, zdim-1, y, 1, ydim-1, x, 1, xdim-1 )
    {
        id_type& id = ids[x][y][z];

        const value_type negx = cmat[x][y][z][0];
        const value_type negy = cmat[x][y][z][1];
        const value_type negz = cmat[x][y][z][2];
        const value_type posx = cmat[x+1][y][z][0];
        const value_type posy = cmat[x][y+1][z][1];
        const value_type posz = cmat[x][y][z+1][2];

        const value_type m = std::max(std::max(std::max(negx,negy),negz),
                             std::max(std::max(posx,posy),posz));

        if ( m >= low )
        {
            if ( negx == m || negx >= high ) { id |= 0x01; }
            if ( negy == m || negy >= high ) { id |= 0x02; }
            if ( negz == m || negz >= high ) { id |= 0x04; }
            if ( posx == m || posx >= high ) { id |= 0x08; }
            if ( posy == m || posy >= high ) { id |= 0x10; }
            if ( posz == m || posz >= high ) { id |= 0x20; }
        }
    }

    const index   dindex[6]   = { 1, xdim, xdim*ydim, -1, -xdim, -xdim*ydim };
    const id_type dirmask[6]  = { 0x08, 0x10, 0x20, 0x01, 0x02, 0x04 };
    const id_type idirmask[6] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20 };

    //toc("Entering BFS/DFS");

    id_type next_id = 1;

    {
        index* bfs       = detail::get_buffer<index>(size+1);
        index  bfs_index = 0;
        index  bfs_start = 0;
        index  bfs_end   = 0;

        QW_FOR_3( index, iz, 1, zdim-1, iy, 1, ydim-1, ix, 1, xdim-1 )
        {
            index idx = &ids[ix][iy][iz] - seg;

            if ( !(seg[ idx ] & assigned_bit ) )
            {
                bfs[ bfs_end++ ] = idx;
                seg[ idx ] |= visited_bit;

                while ( bfs_index != bfs_end )
                {
                    index y = bfs[ bfs_index++ ];

                    for ( index d = 0; d < 6; ++d )
                    {
                        if ( seg[ y ] & dirmask[ d ] )
                        {
                            index z = y + dindex[ d ];

                            if ( seg[ z ] & assigned_bit )
                            {
                                const id_type seg_id = seg[ z ];
                                while ( bfs_start != bfs_end )
                                {
                                    seg[ bfs[ bfs_start++ ] ] = seg_id;
                                }
                                bfs_index = bfs_end;
                                d = 6; // (break)
                            }
                            else if ( !( seg[ z ] & visited_bit ) )
                            {
                                seg[ z ] |= visited_bit;
                                if ( !( seg[ z ] & idirmask[ d ] ) )  // dfs now
                                {
                                    bfs_index = bfs_end;
                                    d = 6; // (break)
                                }
                                bfs[ bfs_end++ ] = z;
                            }
                        }
                    }
                }

                if ( bfs_start != bfs_end )
                {
                    while ( bfs_start != bfs_end )
                    {
                        seg[ bfs[ bfs_start++ ] ] = assigned_bit | next_id;
                    }
                    ++next_id;
                }
            }
        }

        detail::return_buffer(bfs);
    }

    //toc("Counting Segments");

    counts.resize(next_id);

    for ( id_type i = 0; i < static_cast< id_type >( size ); ++i )
    {
        // if the highest bit of seg[i] is set, we remove it, otherwise
        // we set seg[i] to be zero. we avoid branching by using bit opratoins
        seg[i] &= assigned_bit-(seg[i]>>(std::numeric_limits<count_type>::digits-1));
        ++counts[ seg[i] ];
        ZI_ASSERT(seg[i]<next_id);
    }

    // invariant check
    ZI_ASSERT((ids[0][0][0]|ids[xdim-1][0][0])==0);
    ZI_ASSERT((ids[0][ydim-1][0]|ids[xdim-1][ydim-1][0])==0);
    ZI_ASSERT((ids[0][0][zdim-1]|ids[xdim-1][0][zdim-1])==0);
    ZI_ASSERT((ids[0][ydim-1][zdim-1]|ids[xdim-1][ydim-1][zdim-1])==0);

    //toc("Looking for Graph Edges");

    zi::unordered_map< id_pair, value_type > uniques;

    value_type dend_min = std::min( low, merge_low );

    const index rindex[3] = { -1, -xdim, -xdim*ydim };

    for ( index d = 0, eoff = 0; d < 3; ++d, eoff += size )
    {
        QW_FOR_3( index, z, 1, zdim-1, y, 1, ydim-1, x, 1, xdim-1 )
        {
            index i = &ids[x][y][z] - seg;

            if ( conn[ eoff + i ] >= dend_min &&
                 seg[ i ] &&
                 seg[ i + rindex[ d ] ] &&
                 seg[ i + rindex[ d ] ] != seg[ i ] )
            {
                id_pair p = minmax( seg[ i ], seg[ i + rindex[ d ] ] );
                typename zi::unordered_map< id_pair, value_type >::iterator it =
                    uniques.find( p );

                if ( it == uniques.end() )
                {
                    uniques.insert( std::make_pair( p, conn[ eoff + i ] ) );
                }
                else
                {
                    if ( it->second < conn[eoff+i] )
                    {
                        it->second = conn[eoff+i];
                    }
                }
            }
        }
    }

    dendr.resize( uniques.size() );
    index idx = 0;

    FOR_EACH( it, uniques )
    {
        dendr[ idx ] = tie( it->first.first, it->first.second, it->second );
        ++idx;
    }

    // trick to clear memory
    {
        zi::unordered_map< id_pair, value_type > uniques_deleter;
        uniques_deleter.swap(uniques);
    }

    //toc("Sorting Graph Edges");

    std::stable_sort( dendr.begin(), dendr.end(), edge_compare< value_type, id_type > );

    //toc("Preparing the Initial Dendrogram");

    ZI_IF_DEBUG(std::set<id_type> on_border_set);

    QW_FOR_2( index, z, 1, zdim-1, y, 1, ydim-1 )
    {
        --counts[ ids[0][y][z] ];
        --counts[ ids[xdim-1][y][z] ];
        if ( borders & border::before_x )
        {
            ZI_ASSERT((!ids[0][y][z])||(ids[1][y][z]==ids[1][y][z]));
            ZI_ASSERT(on_border_set.insert(ids[0][y][z]).second
                      ||ids[0][y][z]==0||cmat[1][y][z][0]>=high);
            counts[ ids[0][y][z] ] |= on_border;
            counts[ ids[1][y][z] ] |= on_border;
        }

        if ( borders & border::after_x )
        {
            ZI_ASSERT((!ids[xdim-1][y][z])||(ids[xdim-1][y][z]==ids[xdim-2][y][z]));
            ZI_ASSERT(on_border_set.insert(ids[xdim-1][y][z]).second
                      ||ids[xdim-1][y][z]==0||cmat[xdim-1][y][z][0]>=high);
            counts[ ids[xdim-2][y][z] ] |= on_border;
            counts[ ids[xdim-1][y][z] ] |= on_border;
        }
    }

    QW_FOR_2( index, z, 1, zdim-1, x, 1, xdim-1 )
    {
        --counts[ ids[x][0][z] ];
        --counts[ ids[x][ydim-1][z] ];
        if ( borders & border::before_y )
        {
            ZI_ASSERT((!ids[x][0][z])||(ids[x][0][z]==ids[x][1][z]));
            ZI_ASSERT(on_border_set.insert(ids[x][0][z]).second
                      ||ids[x][0][z]==0||cmat[x][1][z][1]>=high);
            counts[ ids[x][0][z] ] |= on_border;
            counts[ ids[x][1][z] ] |= on_border;
        }

        if ( borders & border::after_y )
        {
            ZI_ASSERT((!ids[x][ydim-1][z])||(ids[x][ydim-1][z]==ids[x][ydim-2][z]));
            ZI_ASSERT(on_border_set.insert(ids[x][ydim-1][z]).second||
                      ids[x][ydim-1][z]==0||cmat[x][ydim-1][z][1]>=high);
            counts[ ids[x][ydim-2][z] ] |= on_border;
            counts[ ids[x][ydim-1][z] ] |= on_border;
        }
    }

    QW_FOR_2( index, y, 1, ydim-1, x, 1, xdim-1 )
    {
        --counts[ ids[x][y][0] ];
        --counts[ ids[x][y][zdim-1] ];
        if ( borders & border::before_z )
        {
            ZI_ASSERT((!ids[x][y][0])||(ids[x][y][0]==ids[x][y][1]));
            ZI_ASSERT(on_border_set.insert(ids[x][y][0]).second
                      ||ids[x][y][0]==0||cmat[x][y][1][2]>=high);

            counts[ ids[x][y][0] ] |= on_border;
            counts[ ids[x][y][1] ] |= on_border;
        }

        if ( borders & border::after_z )
        {
            ZI_ASSERT((!ids[x][y][zdim-1])||(ids[x][y][zdim-1]==ids[x][y][zdim-2]));
            ZI_ASSERT(on_border_set.insert(ids[x][y][zdim-1]).second
                      ||ids[x][y][zdim-1]==0||cmat[x][y][zdim-1][2]>=high);
            counts[ ids[x][y][zdim-2] ] |= on_border;
            counts[ ids[x][y][zdim-1] ] |= on_border;
        }
    }

    counts[0] = 0;

    //toc("Going Down The Dendrogram For High/Size Thresholds");

    disjoint_sets< id_type > sets( next_id );

    for ( size_type i = 0; i < dendr.size(); ++i )
    {
        id_type v1 = sets[zi::get<0>(dendr[i])];
        id_type v2 = sets[zi::get<1>(dendr[i])];

        ZI_ASSERT(v1&&v2);

        if ( zi::get<2>(dendr[i]) >= high )
        {
            if ( v1 != v2 )
            {
                counts[v1] += counts[v2]&(~on_border);
                counts[v1] |= counts[v2]&on_border;
                counts[v2]  = 0;
                std::swap( counts[ sets.join(v1,v2) ], counts[v1] );
                --next_id;
            }
        }
        else if ( zi::get<2>(dendr[i]) >= merge_low )
        {
            if ( v1 != v2 )
            {
                if ((counts[v1]<merge_size) || (counts[v2]<merge_size))
                {
                    if ((on_border&(counts[v1]|counts[v2]))==0)
                    {
                        counts[v1] += counts[v2];
                        counts[v2]  = 0;
                        std::swap( counts[ sets.join(v1,v2) ], counts[v1] );
                        --next_id;
                    }
                    else
                    {
                        counts[v1] |= counts[v2]&on_border;
                        counts[v2] |= counts[v1]&on_border;
                    }
                }
            }
        }
        else if ( zi::get<2>(dendr[i]) >= low )
        {
            //counts[v1] |= on_border;
            //counts[v2] |= on_border;
        }
        else
        {
            i = dendr.size();
        }
    }

    //toc("Generating New Segment IDs");

    id_type* remaps = detail::get_buffer<id_type>(counts.size());
    remaps[0] = 0;
    counts[0] &= ~on_border;

    ZI_IF_DEBUG(count_type total_count = 0);

    id_type next_new_id = 1;
    for ( size_type i = 1; i < counts.size(); ++i )
    {
        if ( counts[i]&(~on_border) )
        {
            ZI_ASSERT(sets[i]==static_cast<id_type>(i));

            if ( counts[i] >= dust_size )
            {
                counts[next_new_id] = counts[i]&(~on_border);
                ZI_IF_DEBUG(total_count += counts[next_new_id]);
                remaps[i] = next_new_id;
                ++next_new_id;
            }
            else
            {
                counts[0] += counts[i]&(~on_border);
                ZI_IF_DEBUG(total_count += counts[i]&(~on_border));
                counts[i]  = remaps[i] = 0;
            }
        }
    }

    ZI_ASSERT(static_cast<index>(total_count)==(xdim-2)*(ydim-2)*(zdim-2));

    for ( size_type i = 1; i < counts.size(); ++i )
    {
        remaps[i] = remaps[sets[i]];
    }

    for ( size_type i = 0; i < size; ++i )
    {
        ZI_ASSERT( remaps[ sets[ seg[ i ] ] ] == remaps[ seg[ i ] ] );
        seg[ i ] = remaps[ seg[ i ] ];
    }

    counts.resize( next_new_id );
    sets.resize( next_new_id );

    //toc("Generating the Final Dendrogram");

    index dend_len = 0;
    for ( size_type i = 0; i < dendr.size(); ++i )
    {
        const id_type a1 = remaps[ zi::get<0>(dendr[i]) ];
        const id_type a2 = remaps[ zi::get<1>(dendr[i]) ];

        const id_type v1 = sets[a1];
        const id_type v2 = sets[a2];

        if ( v1 && v2 && v1 != v2 )
        {
            sets.join( v1, v2 );
            dendr[ dend_len++ ] = tie( a1, a2, zi::get< 2 >( dendr[ i ] ) );
        }
    }

    detail::return_buffer(remaps);

    dendr.resize( dend_len );

    //toc("All Done");

    return counts.size();

}

} // namespace watershed
} // namespace zi

#include <quickie/suffix.hpp>

#endif
