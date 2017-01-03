using HDF5
using EMIRT
using BigArrays
using BigArrays.H5sBigArrays

aff = H5sBigArray(ARGS[1])
bbox = boundingbox(aff)
starts = bbox.start
stops = bbox.stop
data_size = collect(bbox.stop - bbox.start + 1)

ba = H5sBigArray(ARGS[2])
#aff = readaff(ARGS[1])
#block_size = size(aff)[1:3]

#f = h5open(ARGS[2], "w")
#
#chunk_size = (256, 256, 32)
#ba = d_create(f, "main", datatype(UInt32),
#    dataspace(block_size[1]-2, block_size[2]-2, block_size[3]-2),
#    "chunk", (chunk_size[1], chunk_size[2], chunk_size[3]),
#    "shuffle", (), "deflate", 3)
#    #"blosc", 5)


chunksizes_file = open("input.chunksizes", "r")
metadata_file = open("input.metadata", "r")
chunks = read(metadata_file,Int32,(5,))[3:5]
println(chunks)
size = chunks[1]*chunks[2]*chunks[3]
println(size)
chunksizes = read(chunksizes_file,Int32,3*size)
#println(chunksizes[1])
println(chunksizes)
close(chunksizes_file)
close(metadata_file)

chunk_id = 1
start_x = starts[1] + 1
cs = [0,0,0]
for x in 1:chunks[1]
    start_y = starts[2] + 1
    for y in 1:chunks[2]
        start_z = starts[3] + 1
        for z in 1:chunks[3]
            println("$start_x,$start_y,$start_z")
            cs = chunksizes[chunk_id:chunk_id+2]
            println(cs)
            println("input.chunks/$(x-1)/$(y-1)/$(z-1)/.seg")
            c = open("input.chunks/$(x-1)/$(y-1)/$(z-1)/.seg")
            chunk_data = Mmap.mmap(c, Array{UInt32,3}, (cs[1],cs[2],cs[3]))
            ba[start_x:start_x+cs[1]-1-2,start_y:start_y+cs[2]-1-2,start_z:start_z+cs[3]-1-2] = chunk_data[2:cs[1]-1,2:cs[2]-1,2:cs[3]-1]
            chunk_id += 3
            close(c)
            start_z += cs[3]-2
        end
        start_y += cs[2]-2
    end
    start_x += cs[1]-2
end
#close(f)
