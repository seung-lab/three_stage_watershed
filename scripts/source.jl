using S3Dicts
using BigArrays

starts = [10240, 7680, 0, 1]
stops = [65016, 43716, 1003, 3]

data_size = collect(stops - starts + 1)[1:3]
println(data_size)

chunk_size = [1026,1026,1004]
chunks = div(data_size,chunk_size).+[1,1,0]
