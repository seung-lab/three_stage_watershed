using EMIRT
using HDF5
using BigArrays
using BigArrays.H5sBigArrays

#aff = readaff(ARGS[1])
#data_size = collect(size(aff)[1:3])

aff = H5sBigArray(ARGS[1])
bbox = boundingbox(aff)
starts = bbox.start
stops = bbox.stop
data_size = collect(bbox.stop - bbox.start + 1)[1:3]

run(`rm -rf input.chunks`)

chunk_size = [512,512,128]
chunks = div(data_size,chunk_size)

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
            aff_data = aff[start_x:stop_x, start_y:stop_y, start_z:stop_z, :]
            aff_size = collect(size(aff_data)[1:3])
            println("size of the chunk $(aff_size[1]), $(aff_size[2]), $(aff_size[3])")
            println("$start_x:$stop_x, $start_y:$stop_y, $start_z:$stop_z")
            start_z = stop_z - 1
            write(aff_file, aff_data)
            write(chunksizes_file, convert(Array{Int32,1}, aff_size))
            run(`mkdir -p input.chunks/$(x-1)/$(y-1)/$(z-1)`)
        end
        start_y = stop_y - 1
    end
    start_x = stop_x - 1
end
write(metadata_file, Int32[32,32])
write(metadata_file, convert(Array{Int32,1},chunks))
close(aff_file)
close(chunksizes_file)
close(metadata_file)
