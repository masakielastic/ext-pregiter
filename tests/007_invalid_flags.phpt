--TEST--
preg_iter_ex rejects unsupported flags
--EXTENSIONS--
pregiter
--FILE--
<?php
try {
    preg_iter_ex('/a/', 'a', PREG_SET_ORDER);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
preg_iter_ex(): Argument #3 ($flags) must be a combination of PREG_OFFSET_CAPTURE and PREG_UNMATCHED_AS_NULL
