header="""memory_initialization_radix=16  ; values defined in hexadecimal
memory_initialization_vector=\n"""


with open("ROMcode.txt",mode="r") as f:
	a = f.readlines()

with open("ROMcode_converted_to_coe.coe",mode="w") as f:
	f.write(
		header+
		',\n'.join([ i.strip() for i in a ])+
		";"
	)
