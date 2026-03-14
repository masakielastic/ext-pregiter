--TEST--
preg_iter_ex does not loop forever on zero-length matches
--EXTENSIONS--
pregiter
--FILE--
<?php
function dump_iter(Iterator $it, int $limit): void {
    $i = 0;
    foreach ($it as $match) {
        var_dump($match[0]);
        if (++$i >= $limit) {
            break;
        }
    }
    echo "--\n";
}

dump_iter(preg_iter_ex('/(?:)/', 'ab'), 5);
dump_iter(preg_iter_ex('/\b/', 'ab cd'), 5);
dump_iter(preg_iter_ex('/^/m', "a\nb"), 5);
dump_iter(preg_iter_ex('/x*/', 'ab'), 5);
?>
--EXPECT--
string(0) ""
string(0) ""
string(0) ""
--
string(0) ""
string(0) ""
string(0) ""
string(0) ""
--
string(0) ""
string(0) ""
--
string(0) ""
string(0) ""
string(0) ""
--
