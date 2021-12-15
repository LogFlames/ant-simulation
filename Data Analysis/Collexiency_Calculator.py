import collections
import csv
import os
import statistics
import math
path = r"C:\Users\ag23662\Documents\Documents\Gymnasiearbete\Myrdata\Gymnasiearbete - Data\40x2^15"
#PARAMETRAR:
simulationsPerAgentNumber = 40
relativeCollectiency = False

mapDictionary = collections.defaultdict(dict)

for simulationName in os.listdir(path):
    agentNumber = int(simulationName.split("_")[2][2:])
    #print(agentNumber)
    with open(path + "\\" + simulationName, 'r') as csv_file:
        csv_reader = csv.reader(csv_file, delimiter = ' ')
        lineCounter = 0
        for line in csv_reader:
            if lineCounter < 10:
                if lineCounter == 3:
                    if "no_walls" in line[1]:
                        currentMap = "no_walls"
                        totalFood = 1877
                    elif "split_path" in line[1]:
                        currentMap = "split_path"
                        totalFood = 1877
                    elif "multifood" in line[1]:
                        currentMap = "multifood"
                        totalFood = 6887
                    else:
                        currentMap = "simplemaze"
                        totalFood = 3241
                elif lineCounter == 4:
                    if "false" in line[1]:
                        currentMap = currentMap.upper()  #UPPERCASE = inga feromoner i mapp
                lineCounter += 1
                continue
            if int(line[1]) >= totalFood * .9 or line[0] == "1602.05":  #har vanligtvis körts på .99
                collectiencyValue = int(line[1]) / float(line[0])
                if relativeCollectiency is True:
                    collectiencyValue /= agentNumber

                if mapDictionary[currentMap].get(agentNumber) is None:
                    mapDictionary[currentMap][agentNumber] = [collectiencyValue]
                else:
                     mapDictionary[currentMap][agentNumber].append(collectiencyValue)

                     if len(mapDictionary[currentMap][agentNumber]) == simulationsPerAgentNumber:
                         collectiencyValueList = mapDictionary[currentMap][agentNumber]
                         averageCollectiency = sum( collectiencyValueList ) / simulationsPerAgentNumber
                         collectiencyStandardDeviation = statistics.pstdev( collectiencyValueList )
                         mapDictionary[currentMap][agentNumber] = (averageCollectiency, collectiencyStandardDeviation)
                break
print( "\n".join([f"{math.log2(k)} " + " ".join(map(lambda x: "{:.20f}".format(x), v[:])) for k, v in mapDictionary["simplemaze".upper()].items() ]))  #printa ut collectiency
