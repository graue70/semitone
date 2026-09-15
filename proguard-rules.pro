# Semitone binds to its native library through the static native methods
# on PianoEngine; their names must survive shrinking and obfuscation.
-keepclasseswithmembernames class mn.tck.semitone.PianoEngine {
    native <methods>;
}
