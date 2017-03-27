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

#ifndef ZI_WATERSHED_XXL_WATERSHED_HPP
#define ZI_WATERSHED_XXL_WATERSHED_HPP 1

#include <detail/config.hpp>
#include <xxl/base.hpp>
#include <xxl/chunk_dimensions.hpp>
#include <quickie/quickie_impl.hpp>
#include <detail/xxl_chunk.hpp>
#include <detail/xxl_queue.hpp>

#include <detail/mmap_file.hpp>
#include <detail/mmap_vector.hpp>
#include <detail/mmap_ostream.hpp>

#include <zi/bits/shared_ptr.hpp>

#include <limits>
#include <list>
#include <algorithm>

namespace zi {
namespace watershed {

namespace xxl {

template< class T, class Id, class Count >
class stage_3_impl:
        watershed_base< T, Id, Count, shared_ptr< detail::xxl_chunk< T, Id, Count > > >
{
public:
    typedef watershed_base< T, Id, Count, shared_ptr< detail::xxl_chunk< T, Id, Count > > > super_type;
    typedef stage_3_impl< T, Id, Count > this_type;

    typedef detail::xxl_chunk< T, Id, Count > xxl_chunk_type;
    typedef shared_ptr< xxl_chunk_type >      chunk_type;

    typedef typename super_type::value_type value_type;
    typedef typename super_type::const_value_type const_value_type;
    typedef typename super_type::value_ptr value_ptr;
    typedef typename super_type::const_value_ptr const_value_ptr;

    typedef typename super_type::id_type id_type;
    typedef typename super_type::const_id_type const_id_type;
    typedef typename super_type::id_ptr id_ptr;
    typedef typename super_type::const_id_ptr const_id_ptr;

    typedef typename super_type::count_type count_type;
    typedef typename super_type::const_count_type const_count_type;

    typedef typename super_type::affinity_volume_ref affinity_volume_ref;
    typedef typename super_type::id_volume_ref id_volume_ref;

    typedef typename super_type::id_pair id_pair;
    typedef typename super_type::edge_type edge_type;
    typedef typename super_type::count_vector count_vector;

    typedef typename super_type::edge_vector edge_vector;
    typedef typename super_type::id_pair_vector id_pair_vector;
    typedef typename super_type::edge_map edge_map;

    typedef unordered_map< id_pair, value_type > edgemap_type;


protected:
    std::vector< count_type > sizes_      ;
    disjoint_sets< id_type  > sets_       ;
    std::vector< value_type > merge_at_   ;
    std::vector< id_type    > reorder_    ;

    size_type tasks_done_                 ;
    id_type   total_num_domains_          ;

    bool verbose_;

public:
    stage_3_impl( const std::string& filename,
                    const value_type&  high_threshold,
                    const value_type&  low_threshold,
                    const count_type&  merge_size_threshold,
                    const value_type&  dust_merge_threshold,
                    const count_type&  dust_threshold,
                    size_type xsize,
                    size_type ysize,
                    size_type zsize,
                    size_type x,
                    size_type y,
                    size_type z,
                    const chunk_dimensions* cdims,
                    bool verbose = true )
        : super_type( filename, high_threshold, low_threshold, merge_size_threshold,
                      dust_merge_threshold, dust_threshold, xsize, ysize, zsize ),
          sizes_(),
          sets_(),
          merge_at_(),
          reorder_(),
          tasks_done_(0),
          total_num_domains_(0),
          verbose_( verbose || detail::ws_always_verbose() )
    {
        std::string aff_file_name = filename + ".affinity.data";
        shared_ptr< boost::interprocess::file_mapping > aff_file
            ( new boost::interprocess::file_mapping
              ( aff_file_name.c_str(),
                boost::interprocess::read_only ));

        size_type foff = 0;

        int flags = 0;
        flags |= x == 0 ? 0 : border::before_x;
        flags |= y == 0 ? 0 : border::before_y;
        flags |= z == 0 ? 0 : border::before_z;
        flags |= x == super_type::xdim() - 1 ? 0 : border::after_x;
        flags |= y == super_type::ydim() - 1 ? 0 : border::after_y;
        flags |= z == super_type::zdim() - 1 ? 0 : border::after_z;

        super_type::chunk( x, y, z ) = chunk_type
            ( new xxl_chunk_type( x, y, z, flags,
                                  cdims[0].x(),
                                  cdims[0].y(),
                                  cdims[0].z(),
                                  filename,
                                  aff_file,
                                  foff * sizeof( T ) * 3, true ));
        reorder_ = super_type::chunk(x,y,z)->load_reorder();
        update_chunk_data(super_type::chunk(x,y,z));
    }

    void update_chunk_data( chunk_type c )
    {
        c->load_chunk();
        id_ptr seg = c->get()->data();

        const size_type chunk_size = c->get()->size();

        for ( size_type i = 0; i < chunk_size; ++i )
        {
            if ( seg[ i ] )
            {
                seg[ i ] = reorder_[ seg[ i ] ];
            }
        }

        c->flush_chunk();

        if ( verbose_ )
        {
            mutex::guard g( super_type::mutex_ );
            std::cout << "\rFlushed chunk: " << (*c) << "\n"
                      << "Updating chunks' segmentation IDs "
                      << (++tasks_done_) << '/' << super_type::size()
                      << std::flush;
        }

    }


    void initial_chunk_quickie( chunk_type c )
    {
        const_value_ptr affinities = c->load_affinities();
        id_ptr          ids        = c->allocate_chunk();

        mmap_vector< count_type >& counts     = c->counts();
        mmap_vector< edge_type  >& dendrogram = c->dendr();

        chunk_quickie_impl< T, Id, Count >
            ( c->xdim(), c->ydim(), c->zdim(),
              affinities,
              super_type::high_threshold(),
              super_type::low_threshold(),
              super_type::merge_threshold(),
              super_type::dust_merge_threshold(),
              super_type::dust_threshold(),
              c->flags(),
              ids,
              counts,
              dendrogram );

        c->flush_chunk( true );
        c->count( counts.size() );
        c->conn()->free_data();

        if ( verbose_ )
        {
            mutex::guard g( super_type::mutex_ );
            std::cout << "\rQuickieWS on chunk " << (*c)
                      << " domains: " << counts.size()
                      << " dendr: " << dendrogram.size() << '\n'
                      << "Initial per chunk Watershed " << (++tasks_done_) << '/'
                      << super_type::size() << std::endl;
        }
        counts.flush();
        dendrogram.flush();
        c->flush_sizes();
    }

};


} // namespace xxl

} // namespace watershed
} // namespace zi

#endif

