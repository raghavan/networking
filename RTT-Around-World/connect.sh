mkdir rkrish20;mkdir data
scp data/ips.txt user@remote.server.com:/rkrish20
for host in `cat data/ips.txt`;do 
echo $host
ssh uic_cs450@$host
done
cd ../..
rm -r rkrish20
