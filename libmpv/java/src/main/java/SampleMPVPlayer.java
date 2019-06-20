import com.sun.jna.Pointer;
import com.sun.jna.platform.win32.User32;
import com.sun.jna.platform.win32.WinDef;
import com.sun.jna.ptr.LongByReference;

import javafx.application.Application;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.paint.Color;
import javafx.stage.Modality;
import javafx.stage.Stage;
import javafx.stage.StageStyle;

public class SampleMPVPlayer extends Application {
  private static final String STAGE_TITLE = "MPV video demo";

  public static void main(String[] args) {
    Application.launch(args);
  }

  @Override
  public void start(Stage stage) throws Exception {
    stage.setTitle(STAGE_TITLE);
    stage.show();

    Stage childStage = new Stage(StageStyle.TRANSPARENT);
    Button button = new Button("Hello World");
    Scene scene = new Scene(button);

    button.setStyle("-fx-font-size: 50px");
    button.setOpacity(0.5);

    scene.setFill(Color.TRANSPARENT);

    childStage.initModality(Modality.APPLICATION_MODAL);
    childStage.initOwner(stage);
    childStage.setScene(scene);
    childStage.show();

    play("https://www.youtube.com/watch?v=sFXGrTng0gQ");
  }

  private void play(String url) {
    // Get interface to MPV DLL
    MPV mpv = MPV.INSTANCE;

    // Create MPV player instance
    long handle = mpv.mpv_create();

    // Get the native window id by looking up a window by title:
    WinDef.HWND hwnd = User32.INSTANCE.FindWindow(null, STAGE_TITLE);

    // Tell MPV on which window video should be displayed:
    LongByReference longByReference =
        new LongByReference(Pointer.nativeValue(hwnd.getPointer()));
    mpv.mpv_set_option(handle, "wid", 4, longByReference.getPointer());

    int error;

    // Initialize MPV after setting basic options:
    if((error = mpv.mpv_initialize(handle)) != 0) {
      throw new IllegalStateException("Initialization failed with error: " + error);
    }

    // Load and play a video:
    if((error = mpv.mpv_command(handle, new String[] {"loadfile", url})) != 0) {
      throw new IllegalStateException("Playback failed with error: " + error);
    }
  }
}
