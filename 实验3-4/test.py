from pykeyboard import *
from pymouse import *
import time
import os

m = PyMouse() #建立鼠标对象
k = PyKeyboard() #建立键盘对象



"""
Router在任务栏的位置：(777, 709)
Router关闭的位置：(909, 136)
左127的位置：(494, 179)
左1的位置：(599, 187)
右127的位置：(738, 184)
右1的位置：(874, 177)
左端口：(469, 223)
右端口：(735, 224)
丢包率：(477, 259)
延时：(743, 259)
确定键的位置：(599, 187)
"""




# time.sleep(2)
routerIcon=(81, 707)
routerClose=(909, 136)
left127=(494, 179)
left1=(599, 187)
right127=(738, 184)
right1=(874, 177)
leftPort=(469, 223)
rightPort=(735, 224)
lostRate=(477, 259)
timeDelay=(743, 259)
yes=(523, 305)

tabs1=(32, 679)
tabc1=(129, 672)
tabs2=(228, 669)
tabc2=(317, 670)
tabs3=(429, 667)
tabc3=(521, 673)
window=(483,593)

serverPort=['1166','1266','1466']
txt=['client1.txt','client2.txt','client3.txt']
exe=['client1.exe','client2.exe','client3.exe']

# 这是跑一次
j=0
q=0

for j in range(3,5):
	for q in range(0,5):

		for i in range(0,3):


			packageLoss=q*5
			timedelay=j*5
			# timedelay=10

			m.click(routerIcon[0],routerIcon[1])
			time.sleep(0.5)

			m.click(left127[0],left127[1])
			m.click(left127[0],left127[1])
			k.type_string('127')
			time.sleep(0.5)

			m.click(left1[0],left1[1])
			m.click(left1[0],left1[1])
			k.type_string('1')
			time.sleep(0.5)


			m.click(right127[0],right127[1])
			m.click(right127[0],right127[1])
			k.type_string('127')
			time.sleep(0.5)

			m.click(right1[0],right1[1])
			m.click(right1[0],right1[1])
			k.type_string('1')
			time.sleep(0.5)


			m.click(leftPort[0],leftPort[1])
			m.click(leftPort[0],leftPort[1])
			k.type_string('1366')
			time.sleep(0.5)

			m.click(rightPort[0],rightPort[1])
			m.click(rightPort[0],rightPort[1])
			k.type_string(serverPort[i])
			time.sleep(0.5)


			m.click(lostRate[0],lostRate[1])
			m.click(lostRate[0],lostRate[1])
			k.type_string(str(packageLoss))
			time.sleep(0.5)

			m.click(timeDelay[0],timeDelay[1])
			m.click(timeDelay[0],timeDelay[1])
			k.type_string(str(timedelay))
			time.sleep(0.5)


			m.click(yes[0],yes[1])

			#################已经设置好了丢包率和时延#####################

			#################运行第一个程序#################

			fo = open(txt[i], "a")
			fo.write("file: 1.jpg,")
			fo.write("Packet loss: "+str(packageLoss)+"%, time delay: "+str(timedelay)+" ms\n")
			fo.close()
			# 每次传完图都删掉再重传
			for kk in range(5):
				print("executing"+str(kk))
				os.system(exe[i])

			#################运行第一个程序#################





			# k.tap_key(k.enter_key)


			m.click(routerClose[0],routerClose[1])
			print("next one")
			time.sleep(1)


			###########################################

# m.click(routerIcon[0],routerIcon[1])
# time.sleep(1)
# m.click(routerClose[0],routerClose[1])



# print("左端口")
# time.sleep(4)
# location2=m.position()

# print(location2)




# k.type_string("Hello World")
# m.click(location2[0],location2[1])

