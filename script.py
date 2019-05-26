import subprocess
import sys
import os

a_results_list = []
n_results_list = []

ftime = open("time.txt", "w+")

for i in range(int(sys.argv[1])):
	args = "./words.exe"
	subprocess.call(args, stdout=ftime)
	with open("../output_a.txt", "r") as oa:
		a_results_list.append(oa.read())
	with open("../output_n.txt", "r") as on:
		n_results_list.append(on.read())

ftime.close()

with open("time.txt", "r") as tf:
	time_f = list(filter(lambda x:"All" in x, tf.read().split('\n')))
print(time_f)
os.remove("time.txt")




