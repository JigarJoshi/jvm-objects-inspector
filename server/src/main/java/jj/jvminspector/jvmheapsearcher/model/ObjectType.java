package jj.jvminspector.jvmheapsearcher.model;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by jigar.joshi on 4/9/16.
 */

public enum ObjectType {
	USER, VM_OBJECT;
	private static Map<String, ObjectType> codeMap = new HashMap<String, ObjectType>() {{
		put("U", USER);
		put("V", VM_OBJECT);
	}};

	public static ObjectType get(String code) {
		return codeMap.get(code);
	}
}
