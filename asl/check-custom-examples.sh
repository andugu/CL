#!/bin/zsh

echo "BEGIN Running Custom Tests"
for f in ../examples/jp_genc_*.asl; do
	rm ../tvm/test.t
    echo "[*]" $(basename "$f")
    ./asl "$f"  > ../tvm/test.t
    ../tvm/tvm ../tvm/test.t < "${f/asl/in}" > ../tvm/out.txt 
    diff "${f/asl/out}" ../tvm/out.txt
done
echo "END Running Custom Tests"
