function [ w, s ] = read_data( fname )

f = fopen(fname, 'r');
out = textscan(f, '%s %s %s %f,%f,%f %s %f');
fclose(f);

w = out{4};
s = out{8};

end

