/* This is a hand-maintained file. Edit pregiter.stub.php as the signature source of truth. */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_preg_iter_ex, 0, 2, Iterator, 0)
	ZEND_ARG_TYPE_INFO(0, pattern, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, subject, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_PregIterator___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_PregIterator_current, 0, 0, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_PregIterator_key arginfo_class_PregIterator_current

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_PregIterator_next, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_PregIterator_valid, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_PregIterator_rewind arginfo_class_PregIterator_next

ZEND_FUNCTION(preg_iter_ex);
ZEND_METHOD(PregIterator, __construct);
ZEND_METHOD(PregIterator, current);
ZEND_METHOD(PregIterator, key);
ZEND_METHOD(PregIterator, next);
ZEND_METHOD(PregIterator, valid);
ZEND_METHOD(PregIterator, rewind);

static const zend_function_entry pregiter_functions[] = {
	ZEND_FE(preg_iter_ex, arginfo_preg_iter_ex)
	ZEND_FE_END
};

static const zend_function_entry pregiterator_methods[] = {
	ZEND_ME(PregIterator, __construct, arginfo_class_PregIterator___construct, ZEND_ACC_PRIVATE)
	ZEND_ME(PregIterator, current, arginfo_class_PregIterator_current, ZEND_ACC_PUBLIC)
	ZEND_ME(PregIterator, key, arginfo_class_PregIterator_key, ZEND_ACC_PUBLIC)
	ZEND_ME(PregIterator, next, arginfo_class_PregIterator_next, ZEND_ACC_PUBLIC)
	ZEND_ME(PregIterator, valid, arginfo_class_PregIterator_valid, ZEND_ACC_PUBLIC)
	ZEND_ME(PregIterator, rewind, arginfo_class_PregIterator_rewind, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};
