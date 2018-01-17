if [ $# -lt 2 ]
then
    echo Usage: $0 input.png output.pbm
else
     convert $1 -colorspace gray -auto-level -threshold 50% -rotate -90 -flip -negate $2
fi
