using HDF5
using EMIRT
using S3Dicts
using BigArrays

include("source.jl")

d = S3Dict("s3://neuroglancer/pinky40_v3/watershed/4_4_40/")
ba = BigArray(d)


target = [parse(Int32, x) for x in ARGS[1:end]]

#chunks = [4,4,1]
#chunk_size = div(data_size, chunks)

println(chunks)

chunksizes_file = open("input.chunksizes", "r")
metadata_file = open("input.metadata", "r")
chunks = read(metadata_file,Int32,(5,))[3:5]
println(chunks)
geo_size = chunks[1]*chunks[2]*chunks[3]
println(geo_size)
chunksizes = read(chunksizes_file,Int32,3*geo_size)
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
            cs = chunksizes[chunk_id:chunk_id+2]
            println(cs)
            if x == (target[1]+1) && y == (target[2]+1) && z == (target[3]+1)
                println("$start_x,$start_y,$start_z")
                println("input.chunks/$(x-1)/$(y-1)/$(z-1)/.seg")
                c = open("input.chunks/$(x-1)/$(y-1)/$(z-1)/.seg")
                chunk_data = Mmap.mmap(c, Array{Int64,3}, (cs[1],cs[2],cs[3]))
                ba[start_x:start_x+cs[1]-1-2,start_y:start_y+cs[2]-1-2,start_z:start_z+cs[3]-1-2] = convert(Array{UInt32,3}, chunk_data[2:cs[1]-1,2:cs[2]-1,2:cs[3]-1])
                close(c)
            end
            chunk_id += 3
            start_z += cs[3]-2
        end
        start_y += cs[2]-2
    end
    start_x += cs[1]-2
end
