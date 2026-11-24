package skui.android;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.os.Build;
import android.view.Gravity;
import android.view.WindowManager;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/** A registered window owner; intentionally has no launcher or recent-task entry. */
public final class ImeActivity extends Activity {
    private NativeIme host;
    private volatile boolean closed;
    private static final String END = new String("end");
    private final BlockingQueue<String> outgoing = new LinkedBlockingQueue<>(128);

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        WindowManager.LayoutParams params = getWindow().getAttributes();
        // Span the display vertically so IME insets include the keyboard height.
        params.width = 1;
        params.height = WindowManager.LayoutParams.MATCH_PARENT;
        params.gravity = Gravity.TOP | Gravity.LEFT;
        params.x = params.y = 0;
        getWindow().setAttributes(params);
        if (Build.VERSION.SDK_INT >= 33) {
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, this::complete);
        }
        final int port = getIntent().getIntExtra("port", 0);
        final String token = getIntent().getStringExtra("token");
        if (port < 1 || port > 65535 || token == null || !token.matches("[0-9a-f]{32}")) {
            complete();
            return;
        }
        new Thread(() -> {
            try (Socket connection = new Socket()) {
                connection.connect(new InetSocketAddress("127.0.0.1", port), 5000);
                connection.setTcpNoDelay(true);
                PrintWriter output = new PrintWriter(connection.getOutputStream(), true);
                output.println("SKUI K " + token);
                new Thread(() -> {
                    try {
                        String line;
                        while ((line = outgoing.take()) != END) {
                            output.println(line);
                            if (output.checkError()) break;
                        }
                    } catch (InterruptedException error) {
                        Thread.currentThread().interrupt();
                    } finally {
                        try { connection.close(); } catch (Exception ignored) { }
                    }
                }, "skui-input-output").start();
                runOnUiThread(() -> {
                    if (closed) return;
                    host = new NativeIme(this, line -> {
                        if (!outgoing.offer(line)) complete();
                    }, this, this::complete);
                    host.start();
                });
                BufferedReader input = new BufferedReader(new InputStreamReader(
                        connection.getInputStream(), StandardCharsets.UTF_8));
                String line;
                while ((line = input.readLine()) != null) {
                    final String command = line;
                    runOnUiThread(() -> { if (!closed && host != null) host.command(command); });
                }
            } catch (Exception error) {
                if (!closed) android.util.Log.e("SkuiIme", "Input bridge disconnected", error);
            } finally {
                runOnUiThread(this::complete);
            }
        }, "skui-input-connection").start();
    }

    private void complete() {
        if (closed) return;
        closed = true;
        if (host != null) host.stop();
        if (!outgoing.offer(END)) {
            outgoing.clear();
            outgoing.offer(END);
        }
        finishAndRemoveTask();
    }

    @Override public void onBackPressed() { complete(); }
    @Override public void onDestroy() { complete(); super.onDestroy(); }
}
