package skui.android;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.function.Consumer;

/** Shared editor for the direct host and the temporary APK host. */
public final class NativeIme {
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Editable text = new SpannableStringBuilder();
    private final WindowManager windows;
    private final InputMethodManager ime;
    private final EditorView view;
    private final Method legacyHeight;
    private final Consumer<String> output;
    private final Activity activity;
    private final Runnable close;
    private long session;
    private boolean attached, editing, scheduled, hadKeyboard;
    private int lastHeight = -1;
    private long hiddenSince, openedAt;
    private boolean stopped;

    private NativeIme(Context context) {
        this(context, line -> { System.out.println(line); System.out.flush(); }, null, () -> System.exit(0));
    }

    public NativeIme(Context context, Consumer<String> output, Activity activity, Runnable close) {
        this.output = output;
        this.activity = activity;
        this.close = close;
        windows = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        ime = (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        view = new EditorView(context);
        Method height = null;
        try {
            height = InputMethodManager.class.getMethod("getInputMethodWindowVisibleHeight");
        } catch (ReflectiveOperationException ignored) { }
        legacyHeight = height;
    }

    private static String hex(String value) {
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        char[] result = new char[bytes.length * 2];
        String digits = "0123456789abcdef";
        for (int i = 0; i < bytes.length; ++i) {
            result[i * 2] = digits.charAt((bytes[i] & 255) >> 4);
            result[i * 2 + 1] = digits.charAt(bytes[i] & 15);
        }
        return new String(result);
    }

    private static String unhex(String value) {
        if (value.length() % 2 != 0) throw new IllegalArgumentException("Invalid UTF-8 payload");
        byte[] bytes = new byte[value.length() / 2];
        for (int i = 0; i < bytes.length; ++i) {
            int a = Character.digit(value.charAt(i * 2), 16);
            int b = Character.digit(value.charAt(i * 2 + 1), 16);
            if (a < 0 || b < 0) throw new IllegalArgumentException("Invalid UTF-8 payload");
            bytes[i] = (byte) ((a << 4) | b);
        }
        return new String(bytes, StandardCharsets.UTF_8);
    }

    private void send(String message) {
        output.accept("SKUI " + message);
    }

    private void report(Throwable error) {
        error.printStackTrace(System.err);
        String detail = error.toString();
        for (Throwable cause = error; cause != null; cause = cause.getCause()) {
            if (cause.toString().contains("Unknown pid=")) {
                detail = "Android " + Build.VERSION.RELEASE
                        + " 拒绝未登记的独立进程创建输入窗口；当前单文件系统输入桥接不兼容此设备。";
                break;
            }
        }
        send("E " + hex(detail));
    }

    // Exercise the actual window-session path without focusing a window or opening the IME.
    private void probe() {
        try {
            if (activity != null) {
                activity.getWindow().getDecorView().post(() -> send("A"));
                return;
            }
            WindowManager.LayoutParams params = new WindowManager.LayoutParams(1, 1,
                    WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                            | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                    PixelFormat.TRANSLUCENT);
            params.setTitle("skui-input-probe");
            windows.addView(view, params);
            attached = true;
            windows.removeViewImmediate(view);
            attached = false;
            send("A");
        } catch (RuntimeException error) {
            report(error);
            send("F");
        }
    }

    private void changed() {
        if (!editing || scheduled) return;
        scheduled = true;
        main.post(() -> {
            scheduled = false;
            if (!editing) return;
            int start = Math.max(0, Selection.getSelectionStart(text));
            int end = Math.max(0, Selection.getSelectionEnd(text));
            send("T " + session + " " + start + " " + end + " " + hex(text.toString()));
            try {
                ime.updateSelection(view, start, end,
                        BaseInputConnection.getComposingSpanStart(text),
                        BaseInputConnection.getComposingSpanEnd(text));
            } catch (RuntimeException error) { report(error); finish(); }
        });
    }

    // Limit by UTF-8 bytes, matching the C++ buffer, without cutting a code point.
    private InputFilter limit(int capacity) {
        return (source, start, end, dest, dstart, dend) -> {
            int used = dest.subSequence(0, dstart).toString().getBytes(StandardCharsets.UTF_8).length
                    + dest.subSequence(dend, dest.length()).toString().getBytes(StandardCharsets.UTF_8).length;
            int accepted = start;
            while (accepted < end) {
                int cp = Character.codePointAt(source, accepted);
                int count = Character.charCount(cp);
                int bytes = cp <= 0x7f ? 1 : cp <= 0x7ff ? 2 : cp <= 0xffff ? 3 : 4;
                if (accepted + count > end || used + bytes > capacity) break;
                used += bytes;
                accepted += count;
            }
            return accepted == end ? null : source.subSequence(start, accepted);
        };
    }

    private void open(long nextSession, int capacity, int start, int end, String value) {
        editing = false;
        session = nextSession;
        text.setFilters(new InputFilter[]{limit(capacity - 1)});
        text.clear();
        text.clearSpans();
        text.append(value);
        Selection.setSelection(text, Math.min(start, text.length()), Math.min(end, text.length()));
        editing = true;
        hadKeyboard = false;
        hiddenSince = 0;
        lastHeight = -1;
        openedAt = android.os.SystemClock.uptimeMillis();
        if (!attached) {
            if (activity != null) {
                activity.setContentView(view);
                activity.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                        | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
            } else {
                WindowManager.LayoutParams params = new WindowManager.LayoutParams(1, 1,
                        WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                        WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                                | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                        PixelFormat.TRANSLUCENT);
                params.gravity = Gravity.TOP | Gravity.LEFT;
                params.x = params.y = 0;
                params.setTitle("skui-system-input");
                params.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                        | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING;
                windows.addView(view, params);
            }
            attached = true;
        }
        view.requestFocus();
        ime.restartInput(view);
        main.postDelayed(this::showKeyboard, 80);
    }

    private void showKeyboard() {
        if (!editing || !attached || !view.hasWindowFocus()) return;
        try { ime.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT); }
        catch (RuntimeException error) { report(error); finish(); }
    }

    private void finish() {
        if (!editing) return;
        BaseInputConnection.removeComposingSpans(text);
        int start = Math.max(0, Selection.getSelectionStart(text));
        int end = Math.max(0, Selection.getSelectionEnd(text));
        send("T " + session + " " + start + " " + end + " " + hex(text.toString()));
        editing = false;
        if (attached) {
            attached = false;
            try { ime.hideSoftInputFromWindow(view.getWindowToken(), 0); }
            catch (RuntimeException error) { report(error); }
            if (activity == null) {
                try { windows.removeViewImmediate(view); }
                catch (RuntimeException error) { report(error); }
            }
        }
        send("V " + session + " 0");
        send("D " + session);
        if (activity != null) close.run();
    }

    public void command(String line) {
        if (stopped) return;
        try {
            String[] args = line.split(" ", -1);
            if (args[0].equals("O") && args.length >= 6) {
                open(Long.parseLong(args[1]), Integer.parseInt(args[2]),
                        Integer.parseInt(args[3]), Integer.parseInt(args[4]), unhex(args[5]));
            } else if (args[0].equals("S") && args.length == 4 && Long.parseLong(args[1]) == session && editing) {
                Selection.setSelection(text, Math.min(Integer.parseInt(args[2]), text.length()),
                        Math.min(Integer.parseInt(args[3]), text.length()));
                changed();
            } else if (args[0].equals("H") && Long.parseLong(args[1]) == session) {
                finish();
            } else if (args[0].equals("P")) {
                probe();
            } else if (args[0].equals("Q")) {
                finish();
                close.run();
            }
        } catch (RuntimeException error) {
            report(error);
            finish();
        }
    }

    private void pollKeyboard() {
        if (stopped) return;
        if (editing && attached) {
            try {
                int height = 0;
                if (Build.VERSION.SDK_INT >= 30) {
                    WindowInsets insets = view.getRootWindowInsets();
                    if (insets != null) height = insets.getInsets(WindowInsets.Type.ime()).bottom;
                }
                if (height == 0 && legacyHeight != null) height = (Integer) legacyHeight.invoke(ime);
                if (height != lastHeight) {
                    lastHeight = height;
                    send("V " + session + " " + height);
                }
                if (height > 0) {
                    hadKeyboard = true;
                    hiddenSince = 0;
                } else if (hadKeyboard) {
                    long now = android.os.SystemClock.uptimeMillis();
                    if (hiddenSince == 0) hiddenSince = now;
                    if (now - hiddenSince > 300) finish();
                } else if (android.os.SystemClock.uptimeMillis() - openedAt > 6000) {
                    report(new IllegalStateException("系统键盘未显示，输入连接已结束"));
                    finish();
                }
            } catch (ReflectiveOperationException | RuntimeException error) {
                report(error);
                finish();
            }
        }
        main.postDelayed(this::pollKeyboard, 80);
    }

    public void start() { pollKeyboard(); send("R"); }

    public void stop() {
        stopped = true;
        finish();
        main.removeCallbacksAndMessages(null);
    }

    private boolean key(KeyEvent event) {
        if (event.getAction() != KeyEvent.ACTION_DOWN) return true;
        int start = Math.max(0, Math.min(Selection.getSelectionStart(text), Selection.getSelectionEnd(text)));
        int end = Math.max(start, Math.max(Selection.getSelectionStart(text), Selection.getSelectionEnd(text)));
        int code = event.getKeyCode();
        if (code == KeyEvent.KEYCODE_ENTER || code == KeyEvent.KEYCODE_BACK) {
            finish();
        } else if (code == KeyEvent.KEYCODE_DEL) {
            if (start == end && start > 0) start = Character.offsetByCodePoints(text, start, -1);
            text.delete(start, end);
            Selection.setSelection(text, start);
            changed();
        } else {
            int cp = event.getUnicodeChar();
            if (cp > 0 && !Character.isISOControl(cp)) {
                String value = new String(Character.toChars(cp));
                text.replace(start, end, value);
                Selection.setSelection(text, Math.min(start + value.length(), text.length()));
                changed();
            }
        }
        return true;
    }

    private final class EditorView extends View {
        EditorView(Context context) {
            super(context);
            setFocusable(true);
            setFocusableInTouchMode(true);
        }
        @Override public boolean onCheckIsTextEditor() { return true; }
        @Override public void onWindowFocusChanged(boolean focused) {
            super.onWindowFocusChanged(focused);
            if (focused && editing) main.post(NativeIme.this::showKeyboard);
        }
        @Override public boolean onKeyPreIme(int code, KeyEvent event) {
            if (code == KeyEvent.KEYCODE_BACK) {
                if (event.getAction() == KeyEvent.ACTION_UP) finish();
                return true;
            }
            return super.onKeyPreIme(code, event);
        }
        @Override public boolean onKeyDown(int code, KeyEvent event) { return key(event); }
        @Override public InputConnection onCreateInputConnection(EditorInfo info) {
            info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
            info.imeOptions = EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                    | EditorInfo.IME_FLAG_NO_FULLSCREEN;
            info.initialSelStart = Math.max(0, Selection.getSelectionStart(text));
            info.initialSelEnd = Math.max(0, Selection.getSelectionEnd(text));
            if (Build.VERSION.SDK_INT >= 30) info.setInitialSurroundingText(text);
            return new BaseInputConnection(this, true) {
                @Override public Editable getEditable() { return text; }
                @Override public boolean commitText(CharSequence value, int cursor) {
                    boolean result = super.commitText(value, cursor); changed(); return result;
                }
                @Override public boolean setComposingText(CharSequence value, int cursor) {
                    boolean result = super.setComposingText(value, cursor); changed(); return result;
                }
                @Override public boolean finishComposingText() {
                    boolean result = super.finishComposingText(); changed(); return result;
                }
                @Override public boolean deleteSurroundingText(int before, int after) {
                    boolean result = super.deleteSurroundingText(before, after); changed(); return result;
                }
                @Override public boolean deleteSurroundingTextInCodePoints(int before, int after) {
                    boolean result = super.deleteSurroundingTextInCodePoints(before, after); changed(); return result;
                }
                @Override public boolean setSelection(int start, int end) {
                    boolean result = super.setSelection(start, end); changed(); return result;
                }
                @Override public boolean sendKeyEvent(KeyEvent event) { return key(event); }
                @Override public boolean performEditorAction(int action) { finish(); return true; }
                @Override public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
                    ExtractedText result = new ExtractedText();
                    result.text = text.toString();
                    result.selectionStart = Math.max(0, Selection.getSelectionStart(text));
                    result.selectionEnd = Math.max(0, Selection.getSelectionEnd(text));
                    result.flags = ExtractedText.FLAG_SINGLE_LINE;
                    return result;
                }
            };
        }
    }

    public static void main(String[] args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            error.printStackTrace(System.err);
            System.out.println("SKUI E " + hex(error.toString()));
            System.out.flush();
            System.exit(1);
        });
        Looper.prepareMainLooper();
        // The executable starts this child as Android's system UID, matching
        // the system context. It does not impersonate an installed application.
        Class<?> activityThread = Class.forName("android.app.ActivityThread");
        Object thread = activityThread.getMethod("systemMain").invoke(null);
        Context context = (Context) activityThread.getMethod("getSystemContext").invoke(thread);
        context = context.createDisplayContext(((DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE)).getDisplay(0));
        if (Build.VERSION.SDK_INT >= 30) {
            context = context.createWindowContext(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY, null);
        }
        NativeIme host = new NativeIme(context);
        new Thread(() -> {
            try (BufferedReader input = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8))) {
                String line;
                while ((line = input.readLine()) != null) {
                    final String command = line;
                    host.main.post(() -> host.command(command));
                }
            } catch (Exception error) {
                host.send("E " + hex(error.toString()));
            } finally {
                host.main.post(() -> { host.finish(); System.exit(0); });
            }
        }, "skui-ime-commands").start();
        host.start();
        Looper.loop();
    }
}
