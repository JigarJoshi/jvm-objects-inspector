package jj.jvminspector.jvmheapsearcher.processor;

import java.util.concurrent.Callable;

/**
 * Created by jigar.joshi on 4/8/16.
 */

public interface Processor extends Callable {
	void processLine(String line);
}
