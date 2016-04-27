package jj.jvminspector.jvmheapsearcher.model;

/**
 * Created by jigar.joshi on 4/9/16.
 */

public class StackTraceElement {
	private String classSignature;
	private String methodName;
	private int methodLineNumber;
	private String fileName;
	private int lineNumber;

	public String getClassSignature() {
		return classSignature;
	}

	public void setClassSignature(String classSignatureParam) {
		this.classSignature = classSignatureParam;
	}

	public String getMethodName() {
		return methodName;
	}

	public void setMethodName(String methodNameParam) {
		this.methodName = methodNameParam;
	}

	public int getMethodLineNumber() {
		return methodLineNumber;
	}

	public void setMethodLineNumber(int methodLineNumberParam) {
		this.methodLineNumber = methodLineNumberParam;
	}

	public String getFileName() {
		return fileName;
	}

	public void setFileName(String fileNameParam) {
		this.fileName = fileNameParam;
	}

	public int getLineNumber() {
		return lineNumber;
	}

	public void setLineNumber(int lineNumberParam) {
		this.lineNumber = lineNumberParam;
	}

	@Override
	public boolean equals(Object o) {
		if (this == o) return true;
		if (o == null || getClass() != o.getClass()) return false;

		StackTraceElement that = (StackTraceElement) o;

		if (methodLineNumber != that.methodLineNumber) return false;
		if (lineNumber != that.lineNumber) return false;
		if (classSignature != null ? !classSignature.equals(that.classSignature) : that.classSignature != null)
			return false;
		if (methodName != null ? !methodName.equals(that.methodName) : that.methodName != null)
			return false;
		return !(fileName != null ? !fileName.equals(that.fileName) : that.fileName != null);

	}

	@Override
	public int hashCode() {
		int result = classSignature != null ? classSignature.hashCode() : 0;
		result = 31 * result + (methodName != null ? methodName.hashCode() : 0);
		result = 31 * result + methodLineNumber;
		result = 31 * result + (fileName != null ? fileName.hashCode() : 0);
		result = 31 * result + lineNumber;
		return result;
	}

	@Override
	public String toString() {
		return "StackTraceElement{" +
				"classSignature='" + classSignature + '\'' +
				", methodName='" + methodName + '\'' +
				", methodLineNumber=" + methodLineNumber +
				", fileName='" + fileName + '\'' +
				", lineNumber=" + lineNumber +
				'}';
	}
}
