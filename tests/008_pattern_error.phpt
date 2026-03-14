--TEST--
preg_iter_ex surfaces pattern compilation failure
--EXTENSIONS--
pregiter
--FILE--
<?php
try {
    preg_iter_ex('/(/', 'abc');
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
Warning: preg_iter_ex(): Compilation failed: %s
preg_iter_ex(): failed to compile pattern
