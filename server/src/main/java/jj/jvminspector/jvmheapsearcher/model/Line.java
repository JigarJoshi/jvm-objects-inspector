package jj.jvminspector.jvmheapsearcher.model;
/**
 * Created by jigar.joshi on 4/9/16.
 */

import java.util.ArrayList;
import java.util.List;

public class Line {
	long id;
	ObjectType objectType;
	boolean created;
	long createTime;
	long destroyTime;
	List<StackTraceElement> stackTraceElementList = new ArrayList<>();

	public ObjectType getObjectType() {
		return objectType;
	}

	public void setObjectType(ObjectType objectTypeParam) {
		this.objectType = objectTypeParam;
	}

	public long getId() {
		return id;
	}

	public void setId(long idParam) {
		this.id = idParam;
	}

	public boolean isCreated() {
		return created;
	}

	public void setCreated(boolean createdParam) {
		this.created = createdParam;
	}

	public long getCreateTime() {
		return createTime;
	}

	public void setCreateTime(long createTimeParam) {
		this.createTime = createTimeParam;
	}

	public long getDestroyTime() {
		return destroyTime;
	}

	public void setDestroyTime(long destroyTimeParam) {
		this.destroyTime = destroyTimeParam;
	}

	public List<StackTraceElement> getStackTraceElementList() {
		return stackTraceElementList;
	}

	public void setStackTraceElementList(List<StackTraceElement>
			                                     stackTraceElementListParam) {
		this.stackTraceElementList = stackTraceElementListParam;
	}

	@Override
	public boolean equals(Object o) {
		if (this == o) return true;
		if (o == null || getClass() != o.getClass()) return false;

		Line line = (Line) o;

		if (id != line.id) return false;
		if (created != line.created) return false;
		if (createTime != line.createTime) return false;
		if (destroyTime != line.destroyTime) return false;
		if (objectType != line.objectType) return false;
		return !(stackTraceElementList != null ? !stackTraceElementList.equals(line.stackTraceElementList) : line.stackTraceElementList != null);

	}

	@Override
	public int hashCode() {
		int result = (int) (id ^ (id >>> 32));
		result = 31 * result + (objectType != null ? objectType.hashCode() : 0);
		result = 31 * result + (created ? 1 : 0);
		result = 31 * result + (int) (createTime ^ (createTime >>> 32));
		result = 31 * result + (int) (destroyTime ^ (destroyTime >>> 32));
		result = 31 * result + (stackTraceElementList != null ? stackTraceElementList.hashCode() : 0);
		return result;
	}
}
