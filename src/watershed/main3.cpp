#include <utility>
#include <zi/arguments.hpp>
//#include <xxl/watershed.hpp>
#include <xxl/stage_3.hpp>

#include <cstddef>
#include <string>

ZiARG_string( filename,  "./test/t1", "watershed input/output" );
ZiARG_double( high, 0.99, "high thold" );
ZiARG_double( low, 0.1, "low thold" );
ZiARG_int32(  merge, 25, "merge thold" );
ZiARG_double( dust_low, 0.1, "dust low thold" );
ZiARG_int32(  dust, 25, "dust thold" );
ZiARG_bool( verbose, true, "show more info during execution" );

struct watershed_task
{
    int32_t affinity_bitsize;
    int32_t id_bitsize      ;
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t i;
    int32_t j;
    int32_t k;
};

int main( int argc, char **argv )
{
    zi::parse_arguments( argc, argv );


    std::string filename = ZiARG_filename;
    std::string f = filename + ".metadata";

    watershed_task ws;
    zi::watershed::mmap_file::read_n( f, &ws, 1 );

    std::size_t total = ws.x * ws.y * ws.z;
    zi::watershed::chunk_dimensions chunk_sizes[ total ];


    f = filename + ".chunksizes";
    zi::watershed::mmap_file::read( f, chunk_sizes, 12 );

    if ( ws.affinity_bitsize == 32 )
    {
        if ( ws.id_bitsize == 32 )
        {
            zi::watershed::xxl::stage_3_impl< float, uint32_t, uint32_t >
                newws( filename,
                       static_cast< float >( ZiARG_high ),
                       static_cast< float >( ZiARG_low ),
                       static_cast< uint32_t >( ZiARG_merge ),
                       static_cast< float >( ZiARG_dust_low ),
                       static_cast< uint32_t >( ZiARG_dust ),
                       ws.x, ws.y, ws.z,
                       ws.i, ws.j, ws.k,
                       chunk_sizes,
                       ZiARG_verbose);

        }
    }

    return 0;

}
