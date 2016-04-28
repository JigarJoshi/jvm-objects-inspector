package jj.jvminspector.jvmheapsearcher.processor.impl;
/**
 * Created by jigar.joshi on 4/8/16.
 */

import com.lithium.flow.util.Logs;
import com.lithium.flow.util.Sleep;

import java.util.concurrent.BlockingQueue;

import org.slf4j.Logger;

import jj.jvminspector.jvmheapsearcher.processor.Processor;


public class StdoutProcessor implements Processor {
	private final BlockingQueue<String> inputQueue;
	private final static Logger log = Logs.getLogger();

	public StdoutProcessor(BlockingQueue<String> inputQueueParam) {
		this.inputQueue = inputQueueParam;
		log.info("initialized StdoutProcessor");
	}

	@Override
	public Object call() throws Exception {
		while (true) {
			if (inputQueue.isEmpty()) {
				Sleep.softly(100L);
				continue;
			}
			processLine(inputQueue.take());
		}
	}

	@Override
	public void processLine(String line) {
		System.out.println(line);
	}
}
