import numpy as np
import matplotlib.pyplot as plt
import csv
import os
import sys

# Constants provided by assignment
INS_CONST = 50
DEL_CONST = 50
MAXKEY_CONST = 2000000

# This is a simple script built to help showcase a basic reading of a csv file, and display a graph using matplotlib. 
# Note, as no header was meant to be created in the assignment spec, as it would break test cases, there are numerous bad types of nameless array access. Please forgive me.
if len(sys.argv) > 1:
    my_path = os.path.abspath(os.path.dirname(__file__))
    csv_fn = os.path.join(my_path, sys.argv[1])
    if os.path.exists(csv_fn):
        data_type_dict = {}

        with open(csv_fn, 'r') as csvfile:    
            plots = csv.reader(csvfile, delimiter=',')
            for row in plots:
                try:
                    if int(row[1]) == INS_CONST and int(row[2]) == DEL_CONST and int(row[3]) == MAXKEY_CONST: #INS = 50, DEL = 50, MAXKEY = 2000000
                        if row[0] not in data_type_dict:
                            data_type_dict[row[0]] = ([],[]) #Create a tuple holding the x,y arrays that will be used to place the points for each line
                        data_type_dict[row[0]][0].append(int(row[4]))
                        data_type_dict[row[0]][1].append(int(row[13]))
                except:
                    pass # If the data type is not happy with being converted to an int, do nothing! What a great process!

        # We now have the actual data in data_type_dict, time to plot!
        plt.title('Assignment 1 Graph')
        palette = plt.get_cmap('Set1')
        plt.xlabel('Thread Count')
        plt.ylabel('Total Throughput')
        
        num = 0 # Allows for color change for each data type
        for datatype, data_tup in data_type_dict.items():
            plt.plot(data_tup[0], data_tup[1], marker='.', color=palette(num), linewidth=1, label=datatype)
            num += 1

        plt.legend()
        plt.show()

    else:
        print('File does not exist. Here is the passed file path: ' + csv_fn)

else:
    print("USAGE: python graph-gen.py *filepath.csv*")
