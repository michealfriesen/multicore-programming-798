import os

# Variable for the test
testTime = 1000
threadCountList = [1, 2, 4, 8]
testTypeList = ["naive", "lock", "faa"]

resultsDict = {}

def bashScriptMaker(threadCount, testTime, testType):
    return """./workload_timed.out %d %d %s | grep increments/s: | cut -d ":" -f2 | awk '{$1=$1};1' """ % (threadCount, testTime, testType)

def main():
    for testType in testTypeList:
        # Bash command we will run to get the results
        for threadCount in threadCountList:
            if testType not in resultsDict:
                resultsDict[testType] = []
            print("Adding thread count %d for test type %s" % (threadCount, testType))
            resultsDict[testType].append(int(os.popen((bashScriptMaker(threadCount, testTime, testType))).read()))
    
    print(resultsDict)
main()
