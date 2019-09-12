import os

# Variable for the test
testTime = 2000
threadCountList = [1, 2, 4, 8, 16, 32, 64, 128, 256]
testTypeList = ["naive", "lock", "faa"]

def bashScriptMaker(threadCount, testTime, testType):
    return """./workload_timed.out %d %d %s | grep increments/s: | cut -d ":" -f2 | awk '{$1=$1};1' """ % (threadCount, testTime, testType)

def main():
    for testType in testTypeList:
        # Bash command we will run to get the results
        for threadCount in threadCountList:
            result = os.popen((bashScriptMaker(threadCount, testTime, testType))).read()
            print(result)

main()