if [ -z "$1" ]
then
   subdir=""
else
   subdir=$1
fi
dir="${0:A:h}" 
alias line='echo "==========================================================="'

source "${dir}/../../venv/bin/activate"

echo Launch local network
nohup python -m pyaseba.examples.network.simple --number 1 >/dev/null 2>&1 &
bg_pid=$!
r=0
sleep 1
for f in ${dir}/*.py; 
do
preamble=`head -n 1 "$f"`
if [[ $preamble =~ "skip" ]]; then
   echo skipping "$f" 
   echo
   continue;
fi

line
echo "$f" 
echo
python "$f"
r=$?
line
echo
if [ $r != 0 ]; then
    break;
fi
done

kill -2 $bg_pid
# trap "kill -2 $bg_pid" 2
wait $bg_pid
if [[ $r -eq 0 ]];
then
echo "DONE"
fi