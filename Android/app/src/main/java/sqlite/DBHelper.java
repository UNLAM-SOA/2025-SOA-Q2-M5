package sqlite;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

public class DBHelper extends SQLiteOpenHelper {
    private static final String DB_NAME = "produccion.db"; // Nombre del archivo
    private static final int DB_VERSION = 1;

    public DBHelper(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        // Se ejecuta UNA SOLA VEZ, la primera vez que se crea la base
        db.execSQL(
                "CREATE TABLE IF NOT EXISTS stop_log (" +
                        "id INTEGER PRIMARY KEY AUTOINCREMENT," +
                        "type TEXT," +
                        "date TEXT)"
        );

        db.execSQL(
                "CREATE TABLE processed_prods_log (" +
                        "id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                        "color TEXT, " +
                        "date TEXT)"
        );
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        // Se ejecuta si cambias DB_VERSION (por ejemplo, para agregar columnas)
        db.execSQL("DROP TABLE IF EXISTS estados");
        onCreate(db);
    }
}
