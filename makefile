compile:
	gcc -o ./bankSimulatorServer bankSimulatorServer.c 
	gcc -o ./bankSimulatorClient bankSimulatorClient.c
clean:
	rm -rf bankSimulatorClient
	rm -rf bankSimulatorServer
