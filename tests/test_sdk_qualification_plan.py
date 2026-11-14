"""Coverage declarations cannot hide missing contracts or invent execution."""
from dataclasses import replace
import pytest
from tools.sdk_qualification import (
    CompilerCoverage, Exclusion, Gate, Plan, Revisions, Row, initial_report,
    load_plan, plan_from_dict, write_plan,
)
from tools.sdk_qualification.policy import BOARDS
from tools.hardware.suites import PSRAM_ASYNC


@pytest.fixture
def plan():
    # Synthetic identities declare intended inputs; they are not Git evidence.
    return Plan(Revisions('a' * 40, 'b' * 40, 'c' * 40))


def test_matrix_and_initial_report(plan):
    report = initial_report(plan)
    assert len(plan.rows) == 20
    assert set(row.board for row in plan.rows) == set(BOARDS)
    assert len(report['requirements']) == 58
    for board in BOARDS:
        for suite in ('startup', 'runtime', 'startup-ownership', 'system-full'):
            assert {(r['side'], r['revision']) for r in report['requirements']
                    if r['requirement'] == f'{board}/gcc/{suite}'} == {
                        ('baseline', 'a' * 40), ('candidate', 'b' * 40)}
    assert {r['requirement'] for r in report['requirements'] if r['kind'] in ('intake', 'host')} == {
        'intake-review', 'candidate-host-gcc', 'candidate-hygiene'}
    assert all(row['status'] == 'incomplete' for row in report['requirements'])
    assert report['status'] == 'incomplete'
    assert report['qualification'] is False and report['executed'] is False


@pytest.mark.parametrize('index', range(20))
def test_missing_row_cannot_be_hidden_by_exclusion(plan, index):
    row = plan.rows[index]
    with pytest.raises(ValueError, match='missing or weakened'):
        replace(plan, rows=tuple(r for r in plan.rows if r != row),
                exclusions=(Exclusion(row.id, 'deferred', 'Board unavailable'),))


@pytest.mark.parametrize('change', [dict(execution='build'), dict(cases=('weaker',)),
                                  dict(comparison='candidate-only', reason='Omit baseline')])
def test_core_floor_cannot_be_weakened(plan, change):
    with pytest.raises(ValueError):
        replace(plan, rows=(replace(plan.rows[0], **change), *plan.rows[1:]))


def test_candidate_only_and_prerequisite_obligations(plan):
    rows = tuple(replace(row, comparison='candidate-only', reason='API absent from baseline')
                 if row.suite == 'system-full' else row for row in plan.rows)
    extra = Row('apollo510_evb', PSRAM_ASYNC.name, PSRAM_ASYNC.cases,
                comparison='candidate-only', reason='New async API', prerequisites=('memory-fixture',))
    with pytest.raises(ValueError, match='Undeclared prerequisite'):
        replace(plan, rows=(*rows, extra))
    revised = replace(plan, rows=(*rows, extra), gates=(*plan.gates,
        Gate('memory-fixture', 'prerequisite', 'Fitted PSRAM and test region verified')))
    report = initial_report(revised)
    assert [r['side'] for r in report['requirements'] if r['requirement'] == extra.id] == ['candidate']
    assert any(r['requirement'] == 'memory-fixture' and r['status'] == 'incomplete'
               for r in report['requirements'])
    assert all(r['side'] == 'candidate' for r in report['requirements']
               if r['requirement'].endswith('/system-full'))
    assert revised.digest != plan.digest
    assert plan_from_dict(revised.to_dict()) == revised


@pytest.mark.parametrize('suite,board', [('psram-async', 'apollo3p_evb'),
                                       ('performance', 'apollo3p_evb'), ('clock', 'apollo510b_evb')])
def test_registry_membership_is_not_qualified_support(plan, suite, board):
    from tools.hardware.suites import SUITES
    definition = next(s for s in SUITES if s.name == suite)
    with pytest.raises(ValueError, match='Unqualified board/suite'):
        replace(plan, rows=(*plan.rows, Row(board, suite, definition.cases)))


def test_harness_drift_does_not_change_policy(plan, monkeypatch):
    from tools.hardware import suites
    monkeypatch.setattr(suites, 'SUITES', tuple(
        replace(s, cases=s.cases[:-1]) if s.name == 'runtime' else s for s in suites.SUITES))
    with pytest.raises(ValueError, match='contract drift'):
        replace(plan)


@pytest.mark.parametrize('gate', ['intake-review', 'candidate-host-gcc', 'candidate-hygiene'])
def test_required_gates(plan, gate):
    with pytest.raises(ValueError, match='required intake/host gate'):
        replace(plan, gates=tuple(g for g in plan.gates if g.id != gate))


def test_deferred_and_compiler_scope_is_explicit_not_passed(plan):
    revised = replace(plan, gates=(*plan.gates,
        Gate('candidate-acfe-abi', 'host', 'Required licensed compiler ABI check')),
        exclusions=(Exclusion('hardware:atomiq110', 'deferred', 'No physical part'),))
    report = initial_report(revised)
    assert report['status'] == 'incomplete' and len(report['exclusions']) == 1
    assert any(r['requirement'] == 'candidate-acfe-abi' and r['status'] == 'incomplete'
               for r in report['requirements'])
    for scope in (plan.rows[0].id, plan.rows[0].id + '/baseline', 'candidate-host-gcc'):
        with pytest.raises(ValueError, match='Exclusions cannot replace'):
            replace(plan, exclusions=(Exclusion(scope, 'deferred', 'Missing coverage'),))


@pytest.mark.parametrize('field', ['rows', 'gates'])
def test_immutable_and_unique_requirements(plan, field):
    with pytest.raises(ValueError):
        replace(plan, **{field: list(getattr(plan, field))})
    with pytest.raises(ValueError, match='Duplicate'):
        replace(plan, **{field: (*getattr(plan, field), getattr(plan, field)[0])})


def test_canonical_identity(plan):
    assert replace(plan, rows=tuple(reversed(plan.rows)), gates=tuple(reversed(plan.gates))).digest == plan.digest
    assert replace(plan, revisions=replace(plan.revisions, candidate='d' * 40)).digest != plan.digest
    gate = Gate('fixture', 'prerequisite', 'First requirement')
    revised = replace(plan, gates=(*plan.gates, gate))
    assert replace(revised, gates=(*plan.gates, replace(gate, description='Changed requirement'))).digest != revised.digest
    assert initial_report(plan)['plan_sha256'] == plan.digest
    value = plan.to_dict()
    value['rows'].clear()
    assert len(plan.rows) == 20


@pytest.mark.parametrize('field,value', [('baseline', 'main'), ('candidate', 'ABCDEF' * 7),
                                       ('harness', None), ('candidate', 'b' * 39)])
def test_exact_revisions(field, value):
    revisions = dict(baseline='a' * 40, candidate='b' * 40, harness='c' * 40)
    revisions[field] = value
    with pytest.raises(ValueError, match='full lowercase'):
        Revisions(**revisions)


@pytest.mark.parametrize('field,value', [('schema_version', 0), ('schema_version', 2),
    ('schema_version', True), ('profile', 'automatic'), ('reset_transport', 'cold-boot'),
    ('rows', {}), ('exclusions', None), ('probe', '123456')])
def test_invalid_or_unknown_plan_fields(plan, field, value):
    data = plan.to_dict()
    data[field] = value
    with pytest.raises(ValueError):
        plan_from_dict(data)


def test_strict_roundtrip(plan, tmp_path):
    path = tmp_path / 'plan.json'
    write_plan(plan, path)
    assert load_plan(path).to_dict() == plan.to_dict()
    assert load_plan(path).digest == plan.digest
    with pytest.raises(FileExistsError):
        write_plan(plan, path)
    path.write_text('{"schema_version":1,"schema_version":1}')
    with pytest.raises(ValueError, match='Duplicate JSON'):
        load_plan(path)
    data = plan.to_dict()
    data['rows'][0]['serial'] = '123456'
    with pytest.raises(ValueError):
        plan_from_dict(data)


def test_gate_and_execution_ids_cannot_alias(plan):
    ids = (plan.rows[0].id, plan.rows[0].id + '/candidate', plan.compilers[0].ids[0])
    for name in ids:
        with pytest.raises(ValueError, match='must not collide'):
            replace(plan, gates=(*plan.gates, Gate(name, 'host', 'Different obligation')))


def test_required_gate_semantics_cannot_be_relabelled(plan):
    gates = tuple(replace(g, description='No review necessary') for g in plan.gates)
    with pytest.raises(ValueError, match='changed required'):
        replace(plan, gates=gates)


def test_multiple_prerequisites_roundtrip_without_changing_equality(plan):
    gates = (*plan.gates, Gate('z-fixture', 'prerequisite', 'Z'), Gate('a-fixture', 'prerequisite', 'A'))
    extra = Row('apollo510_evb', PSRAM_ASYNC.name, PSRAM_ASYNC.cases,
                comparison='candidate-only', reason='New API', prerequisites=('z-fixture', 'a-fixture'))
    revised = replace(plan, rows=(*plan.rows, extra), gates=gates)
    assert plan_from_dict(revised.to_dict()) == revised
    assert plan_from_dict(revised.to_dict()).digest == revised.digest


@pytest.mark.parametrize('toolchain', ['atfe', 'acfe'])
def test_licensed_compiler_obligations_are_typed_build_only(plan, toolchain):
    compiler = CompilerCoverage(toolchain, ('apollo510_evb', 'apollo330mP_evb'), ('link', 'abi'))
    revised = replace(plan, compilers=(*plan.compilers, compiler))
    report = initial_report(revised)
    obligations = [r for r in report['requirements'] if r['requirement'] in compiler.ids]
    assert len(obligations) == 4
    assert all(r['kind'] == 'compiler' and r['side'] == 'candidate' and r['status'] == 'incomplete'
               for r in obligations)
    assert revised.digest != plan.digest
    assert plan_from_dict(revised.to_dict()) == revised
    assert len({(r['requirement'], r['side']) for r in report['requirements']}) == len(report['requirements'])


@pytest.mark.parametrize('compilers', [(),
    (CompilerCoverage('gcc', ('apollo510_evb',), ('build', 'link', 'abi')),),
    (CompilerCoverage('gcc', BOARDS, ('build', 'link')),),
    (CompilerCoverage('atfe', BOARDS, ('build', 'link', 'abi')),),
])
def test_required_compiler_coverage_cannot_shrink(plan, compilers):
    with pytest.raises(ValueError, match='Required GCC'):
        replace(plan, compilers=compilers)


@pytest.mark.parametrize('toolchain,boards,checks', [
    ('clang', BOARDS, ('build',)), ('atfe', (), ('abi',)),
    ('acfe', BOARDS, ('hardware',)), ('gcc', BOARDS, ('build', 'build')),
])
def test_invalid_compiler_declarations(toolchain, boards, checks):
    with pytest.raises(ValueError):
        CompilerCoverage(toolchain, boards, checks)


def test_compiler_scope_and_unique_identity(plan):
    with pytest.raises(ValueError, match='Duplicate compiler'):
        replace(plan, compilers=(*plan.compilers, plan.compilers[0]))
    with pytest.raises(ValueError, match='outside'):
        replace(plan, compilers=(*plan.compilers, CompilerCoverage('atfe', ('wrong_board',), ('abi',))))
    with pytest.raises(ValueError, match='Exclusions cannot replace'):
        replace(plan, exclusions=(Exclusion(plan.compilers[0].ids[0], 'deferred', 'Compiler unavailable'),))
