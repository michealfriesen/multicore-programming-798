import os
import numpy as np
import matplotlib.pyplot as plt

# Variable for the test
testTime = 1000
threadCountList = [1, 2, 4, 8]
testTypeList = ["shard_lock", "shard_wf"]

resultsDict = {}

def bashScriptMaker(threadCount, testTime, testType):
    return """./workload_timed.out %d %d %s | grep increments/s: | cut -d ":" -f2 | awk '{$1=$1};1' """ % (threadCount, testTime, testType)

def graphMaker(dataDict):
    plt.title('Experiment Results')
    palette = plt.get_cmap('Set1')
    plt.xlabel('Thread Count')
    plt.ylabel('Total Throughput')
    
    num = 0 # Allows for color change for each data type
    for dataType, data in dataDict.items():
        plt.plot(data[0], data[1], marker='.', color=palette(num), linewidth=1, label=dataType)
        num += 1

    plt.legend()
    plt.show()

def main():
    for testType in testTypeList:
        # Bash command we will run to get the results
        for threadCount in threadCountList:
            if testType not in resultsDict:
                resultsDict[testType] = []
            print("Adding thread count %d for test type %s" % (threadCount, testType))
            resultsDict[testType].append((int(os.popen((bashScriptMaker(threadCount, testTime, testType))).read()), threadCount))
    graphMaker(resultsDict)
main()
