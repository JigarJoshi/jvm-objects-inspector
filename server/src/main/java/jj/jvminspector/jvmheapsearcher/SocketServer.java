package jj.jvminspector.jvmheapsearcher;
/**
 * Created by jigar.joshi on 3/30/16.
 */

import com.lithium.flow.config.Config;
import com.lithium.flow.util.Logs;
import com.lithium.flow.util.Main;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

import org.slf4j.Logger;

import jj.jvminspector.jvmheapsearcher.handler.RequestHandler;


public class SocketServer extends Thread {
	private ServerSocket serverSocket;
	private final Config config;
	private final int port;
	private static final Logger log = Logs.getLogger();

	public SocketServer(Config config) {
		this.config = config;
		this.port = config.getInt("server.port", 9000);
	}

	public void startServer() {
		try {
			serverSocket = new ServerSocket(port);
			this.start();
		} catch (IOException ioException) {
			log.error("failed to to start server", ioException);
		}
	}

	@Override
	public void run() {
		while (true) {
			try {
				Socket socket = serverSocket.accept();
				RequestHandler requestHandler = new RequestHandler(socket, config);
				requestHandler.start();
			} catch (IOException ioException) {
				log.error("Failed to accept connection", ioException);
			}
		}
	}

	public static void main(String[] args) throws IOException {
		Config appConfig = Main.config();
		int port = appConfig.getInt("server.port", 9000);
		log.info("starting server on port: {}", port);
		SocketServer server = new SocketServer(appConfig);
		server.startServer();
		log.info("server started");
	}
}

