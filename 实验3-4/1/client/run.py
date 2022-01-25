import os
import time

fo = open("client.txt", "a")
fo.write("###################### file: 1.jpg #######################################################\n")
fo.write("###################### Packet loss: 5 %, time delay: 0 ms #####################\n")
fo.close()

# 每次传完图都删掉再重传
for i in range(30):
	print("executing"+str(i))
	os.system('client.exe')
