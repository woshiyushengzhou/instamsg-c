gcc stdoutsub.c -I ../../src -I ../../src/linux -I ../../../MQTTPacket/src MQTTClient.c ../../src/linux/MQTTLinux.c ../../../MQTTPacket/src/MQTTFormat.c  ../../../MQTTPacket/src/MQTTPacket.c ../../../MQTTPacket/src/MQTTDeserializePublish.c ../../../MQTTPacket/src/MQTTConnectClient.c ../../../MQTTPacket/src/MQTTSubscribeClient.c ../../../MQTTPacket/src/MQTTSerializePublish.c -o stdoutsub ../../../MQTTPacket/src/MQTTConnectServer.c ../../../MQTTPacket/src/MQTTSubscribeServer.c ../../../MQTTPacket/src/MQTTUnsubscribeServer.c ../../../MQTTPacket/src/MQTTUnsubscribeClient.c
