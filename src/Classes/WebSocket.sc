
WebSocketClient {
	var <host;
	var <port;
	// we identify resources of the ffi using uuids
	var <uuid;

	var <connected = false;
	var <>onMessage;

	classvar ffi;

	*initClass {
		Class.initClassTree(Gluon);
		ffi = Gluon.open(
			pathToLibrary: PathName.new("".resolveRelative).parentPath +/+ "sclang_websocket.gluon",
		);
	}

	*new {|host="127.0.0.1", port=8765|
		var uuid = UniqueID.next;
		ffi.clientInit(uuid, host, port);
		^super.newCopyArgs(host, port, uuid).init;
	}

	init {
		ffi.clientMessageReceivedCallback(uuid, callback: {|message| this.prMessageReceived(message)});
	}

	connect {|onConnectionChange|
		ffi.clientConnect(uuid, callback: {|connectionStatus|
			"Connection status is now %".format(connectionStatus).postln;
			connected = connectionStatus;
			onConnectionChange.value(connectionStatus);
		});
	}

	sendMessage {|message|
		if(connected.not, {
			"Can only send a message on an open connection".warn;
			^this;
		});
		ffi.clientSendMessage(uuid, message.isKindOf(String), message);
	}

	prMessageReceived {|message|
		"Client has received a message %".format(message).postln;
		onMessage.value(message);
	}

	close {
		if(connected.not, {
			"Can only close an open connection".warn;
			^this;
		});

		ffi.clientCloseConnection(uuid);
		connected = false;
	}
}
