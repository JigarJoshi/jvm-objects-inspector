public class HeapTracker {


	private static native void _newobj(Object thread, Object o);

	private static int engaged = 0;

	public static void newobj(Object o) {
		if (engaged != 0) {
			_newobj(Thread.currentThread(), o);
		}
	}

	private static native void _newarr(Object thread, Object a);

	private static native void _boo();

	public static void newarr(Object a) {
		if (engaged != 0) {
			_newarr(Thread.currentThread(), a);
		}
	}

}

