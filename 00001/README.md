[![cogroid.com](https://github.com/cogroid/resources/raw/main/images/banner/cogroid-48.png)](https://cogroid.com)

# [00001] SchemeEval crashes on Android

SchemeEval run smoothly on x64 and i386 but it crashes on armv7-a.

### Steps to reproduce bug

1. Download [datomspace-tester.apk](https://github.com/cogroid/b-obstacles/releases/download/obstacle-00001/datomspace-tester.apk)

2. Install datomspace-tester.apk (Do not run!)

3. Go to Settings -> Apps -> dAtomSpace Tester. Set Storage permission.

4. Run dAtomSpace Tester

5. App runs about 30 seconds, then it crashes. There is [tombstone file](https://github.com/cogroid/b-obstacles/files/8771985/tombstone_04.txt).

6. View results in datomspace-test.txt file in Download folder

### Source codes

[dAtomSpace Tester](https://github.com/cogroid/b-obstacles/releases/download/obstacle-00001/datomspace-tester.zip)

[dAtomSpace](https://github.com/cogroid/d-atomspace)

##### SchemeEval.java

```
...

package com.cogroid.atomspace;

public class SchemeEval extends GenericEval {
...

	public SchemeEval(AtomSpace as) {
		super();
		jni_ptr = jni_init(as.jni_ptr);
	}

	private native long jni_init(long as_jni_ptr);

	// Call before first use.
	public static void init_scheme() {
		jni_init_scheme();
	}

	private static native void jni_init_scheme();
  
... 
```

##### com_cogroid_atomspace_SchemeEval.h

```
...
JNIEXPORT jlong JNICALL Java_com_cogroid_atomspace_SchemeEval_jni_1init
  (JNIEnv *, jobject, jlong);
  
...
JNIEXPORT void JNICALL Java_com_cogroid_atomspace_SchemeEval_jni_1init_1scheme
  (JNIEnv *, jclass);

...
```

##### com_cogroid_atomspace_SchemeEval.cc

```
...
JNIEXPORT jlong JNICALL Java_com_cogroid_atomspace_SchemeEval_jni_1init
  (JNIEnv *env, jobject thisObj, jlong as_jni_ptr) {
	opencog::AtomSpace *asp = NULL;
	if (as_jni_ptr != 0) {
		cogroid::SPW<opencog::AtomSpace> *spw_asp = cogroid::SPW<opencog::AtomSpace>::get(as_jni_ptr);
		asp = spw_asp->get();
	}
	cogroid::SPW<opencog::SchemeEval> *spw_se = new cogroid::SPW<opencog::SchemeEval>(asp);
	return spw_se->instance();
}

...
JNIEXPORT void JNICALL Java_com_cogroid_atomspace_SchemeEval_jni_1init_1scheme
  (JNIEnv *env, jclass clz) {
	opencog::SchemeEval::init_scheme();
}

...
```

##### Tester.java

```
...
public void testSchemeEval() {
	try {
		String log = "\n===== SchemeEval =====\n";
		writeLog(log);
		try {
			SchemeEval.init_scheme();
		} catch (Throwable e) {
			writeLog(Loader.me().stackTrace(e));
		}
		AtomSpace pv = new AtomSpace();
		SchemeEval se = new SchemeEval(pv);
		String tmpFolder = new java.io.File(_logFile).getParentFile().getAbsolutePath();
		extractScmFiles();
		java.util.List<String> files = scmFiles();
		for (int i = 0; i < files.size(); i++) {
			String fn = files.get(i);
			String text = readTextFile(fn, tmpFolder);
			try {
				writeLog("----- Eval: " + fn + " -----");
				String rs = "";
				se.begin_eval();
				writeLog("begin_eval();");
				se.eval_expr(text);
				writeLog("eval_expr();");
				rs = se.poll_result();
				writeLog("poll_result();");
				//String rs = se.eval(text);
				writeLog(rs);
			} catch (Throwable e) {
				writeLog(Loader.me().stackTrace(e));
			}
		}
	} catch (Throwable e) {
		writeLog(Loader.me().stackTrace(e));
	}
    }
...
```

---
[Head icons created by Freepik - Flaticon](https://www.flaticon.com/free-icons/head)
