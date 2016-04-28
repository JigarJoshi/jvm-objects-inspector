package jj.jvminspector.jvmheapsearcher.processor.impl;
/**
 * Created by jigar.joshi on 4/8/16.
 */

import com.lithium.flow.config.Config;
import com.lithium.flow.table.ElasticTable;
import com.lithium.flow.table.Key;
import com.lithium.flow.table.Row;
import com.lithium.flow.util.ElasticUtils;
import com.lithium.flow.util.Logs;
import com.lithium.flow.util.Main;
import com.lithium.flow.util.Sleep;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;

import org.elasticsearch.client.Client;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import jj.jvminspector.jvmheapsearcher.model.Line;
import jj.jvminspector.jvmheapsearcher.model.ObjectType;
import jj.jvminspector.jvmheapsearcher.model.StackTraceElement;
import jj.jvminspector.jvmheapsearcher.processor.Processor;


public class ElasticsearchProcessor implements Processor {
	private final static Logger log = Logs.getLogger();
	private BlockingQueue<String> inputQueue;
	private Gson gson;
	private Client client;
	private String index;
	private ElasticTable table;

	public ElasticsearchProcessor(BlockingQueue<String> inputQueueParam) throws IOException {
		this.inputQueue = inputQueueParam;
		this.gson = new GsonBuilder().setPrettyPrinting().create();
		Config config = Main.config();
		client = ElasticUtils.buildClient(config);
		index = config.prefix("elastic").getString("index");
		table = new ElasticTable(client, index, "memory");
		log.info("initialized ElasticsearchProcessor");
	}

	@Override
	public void processLine(String lineStr) {
		Line line = parseLine(lineStr);
	}

	@Override
	public Object call() throws Exception {
		while (true) {
			if (inputQueue.isEmpty()) {
				Sleep.softly(100L);
				continue;
			}
			Line line = parseLine(inputQueue.take());
			Row row = createRow(line);
			table.putRow(row);
		}
	}

	private Row createRow(Line line) {
		Row row = new Row(Key.of(line.getId()));
		row.putCell("id", line.getId());
		row.putCell("objectType", line.getObjectType() != null ? line.getObjectType().name
				() : "");
		row.putCell("created", line.isCreated());
		row.putCell("createdTime", line.getCreateTime());
		row.putCell("destroyTime", line.getDestroyTime());
		row.putCell("stackTraceElementList", line.getStackTraceElementList());
		return row;
	}

	private Line parseLine(String lineStr) {
		Line line = new Line();
		String[] data = lineStr.split("_");
		if (data == null || data.length == 0) {
			throw new IllegalArgumentException("Could not parse line ");
		}
		line.setCreated("c".equals(data[0]));
		line.setId(Long.parseLong(data[1]));
		if (line.isCreated()) {
			line.setObjectType(ObjectType.get(data[2]));
			line.setCreateTime(Long.parseLong(data[3]));
			line.setStackTraceElementList(parseStackTraceElement(data[4]));
		} else {
			if (data.length > 2) {
				line.setDestroyTime(Long.parseLong(data[2]));
			}
		}
		return line;
	}

	private List<StackTraceElement> parseStackTraceElement(String str) {
		List<StackTraceElement> result = new ArrayList<>();
		String[] stackTraceElementsStr = str.split(",");

		for (String stackTraceElementStr : stackTraceElementsStr) {
			try {
				StackTraceElement stackTraceElement = new StackTraceElement();
				int start = 0;
				int end = stackTraceElementStr.indexOf('.');
				stackTraceElement.setClassSignature(stackTraceElementStr.substring(0, end));
				start = end + 1;
				end = stackTraceElementStr.indexOf('@', start);
				stackTraceElement.setMethodName(stackTraceElementStr.substring(start, end));
				start = end + 1;
				end = stackTraceElementStr.indexOf('[');
				stackTraceElement.setMethodLineNumber(Integer.parseInt(stackTraceElementStr
						.substring(start, end)));

				start = end + 1;
				end = stackTraceElementStr.indexOf(':');
				stackTraceElement.setFileName(stackTraceElementStr.substring(start, end));

				start = end + 1;
				end = stackTraceElementStr.indexOf(']');
				stackTraceElement.setLineNumber(Integer.parseInt(stackTraceElementStr
						.substring(start, end)));
				result.add(stackTraceElement);
			} catch (Exception ex) {
				log.warn("failed to parse " + stackTraceElementsStr);
			}
		}
		return result;
	}
}