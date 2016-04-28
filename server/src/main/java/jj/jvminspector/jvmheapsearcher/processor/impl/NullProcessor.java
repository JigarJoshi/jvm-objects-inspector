package jj.jvminspector.jvmheapsearcher.processor.impl;
/**
 * Created by jigar.joshi on 4/27/16.
 */

import com.lithium.flow.util.Logs;
import com.lithium.flow.util.Sleep;

import java.util.concurrent.BlockingQueue;

import org.slf4j.Logger;

import jj.jvminspector.jvmheapsearcher.processor.Processor;

public class NullProcessor implements Processor {
	private static final Logger log = Logs.getLogger();
	private BlockingQueue<String> inputQueue;

	public NullProcessor(BlockingQueue<String> inputQueue) {
		this.inputQueue = inputQueue;
		log.info("initialized NullProcessor");
	}

	@Override
	public void processLine(String line) {

	}

	@Override
	public Object call() throws Exception {
		while (true) {
			if (inputQueue.isEmpty()) {
				Sleep.softly(100L);
				continue;
			}
			inputQueue.take();
		}
	}
}
