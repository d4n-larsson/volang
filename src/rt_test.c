/* expect / expect_eq: on failure, record it and return to the test runner's recovery point
 * (ported from src/rt_test.s, spec 008). */
#include "vo.h"

long tf_node;     /* the failing expect call */
long tf_msg;      /* Str * */
long tf_expected; /* rendered values (Str *) or 0 */
long tf_actual;

void rt_expect(long cond, Node *node)
{
    if (cond) return;
    tf_msg = (long)str_from_cstr("`expect` failed: the condition is false.");
    tf_node = (long)node;
    tf_expected = 0;
    tf_actual = 0;
    rt_recover(1);
}

void rt_expect_eq(long actual, long expected, Node *node)
{
    Type *type = ((Node *)node->b.v->data[0].p)->type; /* the first argument's type */
    if (values_equal(actual, expected, type)) return;
    GC_ROOT(&actual);
    GC_ROOT(&expected);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    render_value(sb, actual, type);
    tf_actual = (long)sb_finish(sb);
    sb = sb_new();
    render_value(sb, expected, type);
    tf_expected = (long)sb_finish(sb);
    GC_UNROOT(3);
    tf_msg = (long)str_from_cstr("`expect_eq` failed: the values differ.");
    tf_node = (long)node;
    rt_recover(1);
}
