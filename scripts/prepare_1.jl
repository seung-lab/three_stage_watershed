using EMIRT
using HDF5
using S3Dicts
using BigArrays

d = S3Dict("s3://neuroglancer/pinky40_v3/affinitymap-jnet/4_4_40/")
aff = BigArray(d)
#aff = ba[36353+2048:36864+2048+512+1024, 58369+2048:58880+2048+512+1024, 321-64:384, 1:3]

starts = [36353, 58369, 1, 1]
#starts[3] += 21
stops = starts.+[8000,8000,1075,3]
data_size = collect(stops - starts + 1)[1:3]
println(data_size)

#run(`rm -rf input.chunks`)

chunk_size = [1024,1024,256]
chunks = div(data_size,chunk_size).+1

target = [parse(Int32, x) for x in ARGS[1:end]]

#chunks = [4,4,1]
#chunk_size = div(data_size, chunks)

println(chunks)

aff_file = open("input.affinity.data", "w+")
chunksizes_file = open("input.chunksizes", "w+")
metadata_file = open("input.metadata", "w+")
start_x = starts[1]
for x in 1:chunks[1]
    stop_x = start_x + chunk_size[1] - 1
    if x == chunks[1]
        stop_x = stops[1]
    end
    start_y = starts[2]
    for y in 1:chunks[2]
        stop_y = start_y + chunk_size[2] - 1
        if y == chunks[2]
            stop_y = stops[2]
        end
        start_z = starts[3]
        for z in 1:chunks[3]
            stop_z = start_z + chunk_size[3] - 1
            if z == chunks[3]
                stop_z = stops[3]
            end
            if x == (target[1]+1) && y == (target[2]+1) && z == (target[3]+1)
                aff_data = aff[start_x:stop_x, start_y:stop_y, start_z:stop_z, 1:3]
                aff_size = collect(size(aff_data)[1:3])
                println("size of the chunk $(aff_size[1]), $(aff_size[2]), $(aff_size[3])")
                println("$start_x:$stop_x, $start_y:$stop_y, $start_z:$stop_z")
                write(aff_file, aff_data)
                write(chunksizes_file, convert(Array{Int32,1}, aff_size))
                run(`mkdir -p input.chunks/$(x-1)/$(y-1)/$(z-1)`)
            end
            start_z = stop_z - 1
        end
        start_y = stop_y - 1
    end
    start_x = stop_x - 1
end
write(metadata_file, Int32[32,32])
write(metadata_file, convert(Array{Int32,1},chunks))
write(metadata_file, target)
close(aff_file)
close(chunksizes_file)
close(metadata_file)
