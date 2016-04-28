package jj.jvminspector.jvmheapsearcher.handler;
/**
 * Created by jigar.joshi on 4/8/16.
 */

import com.lithium.flow.config.Config;
import com.lithium.flow.util.Logs;
import com.lithium.flow.util.Threader;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.concurrent.LinkedBlockingQueue;

import org.slf4j.Logger;

import jj.jvminspector.jvmheapsearcher.processor.Processor;
import jj.jvminspector.jvmheapsearcher.processor.ProcessorFactory;

public class RequestHandler extends Thread {
	private final Socket socket;
	private final LinkedBlockingQueue<String> queue;
	private final Processor processor;
	private final Config config;
	private static final Logger log = Logs.getLogger();

	public RequestHandler(Socket socketArg, Config config) throws IOException {
		this.config = config;
		this.socket = socketArg;
		this.queue = new LinkedBlockingQueue<>();
		this.processor = ProcessorFactory.getProcessor(queue, config);
		int requestHandlerConcurrency = config.getInt("request.handler.concurrency", 2);
		new Threader(requestHandlerConcurrency).submit(this.processor.getClass().getName(), processor);

	}

	@Override
	public void run() {
		BufferedReader reader = null;
		PrintWriter writter = null;
		try {
			log.info("ready for request to handle");
			// Get input and output streams
			reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
			writter = new PrintWriter(socket.getOutputStream());
			String line;
			while ((line = reader.readLine()) != null) {
				queueLine(line);
			}
			log.info("closing connection");
			socket.close();
		} catch (Exception e) {
			log.error("Failed to read incoming data");
		} finally {
			try {
				reader.close();
			} catch (Exception ignore) {log.warn("failed to close reader", ignore);}
			try {
				writter.close();
			} catch (Exception ignore) {log.warn("failed to close writter", ignore);}
		}
	}

	private void queueLine(String line) {
		queue.add(line);
	}
}
