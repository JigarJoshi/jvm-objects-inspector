package jj.jvminspector.jvmheapsearcher.processor;
/**
 * Created by jigar.joshi on 4/27/16.
 */

import com.lithium.flow.config.Config;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;

import jj.jvminspector.jvmheapsearcher.processor.impl.ElasticsearchProcessor;
import jj.jvminspector.jvmheapsearcher.processor.impl.NullProcessor;
import jj.jvminspector.jvmheapsearcher.processor.impl.StdoutProcessor;

public class ProcessorFactory {
	public static Processor getProcessor(BlockingQueue<String> queue, Config config) throws IOException {
		switch (config.getString("processor.type", "null")) {
			case "elasticsearch":
				return new ElasticsearchProcessor(queue);
			case "stdout":
				return new StdoutProcessor(queue);
			case "default":
				return new NullProcessor(queue);
		}
		return new NullProcessor(queue);
	}
}
