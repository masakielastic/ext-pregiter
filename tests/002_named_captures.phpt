--TEST--
preg_iter_ex duplicates named captures
--EXTENSIONS--
pregiter
--FILE--
<?php
$it = preg_iter_ex('/(?P<word>[a-z]+)/', 'abc 123');
foreach ($it as $match) {
    var_dump($match);
}
?>
--EXPECT--
array(3) {
  [0]=>
  string(3) "abc"
  ["word"]=>
  string(3) "abc"
  [1]=>
  string(3) "abc"
}
