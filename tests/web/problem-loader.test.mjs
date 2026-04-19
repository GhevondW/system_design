import test from 'node:test';
import assert from 'node:assert/strict';

import { ProblemLoader } from '../../web/js/problem-loader.js';

test('ProblemLoader: getProblems lists all bundled problems with required fields', () => {
    const loader = new ProblemLoader();
    const problems = loader.getProblems();
    assert.ok(problems.length >= 3, 'expected at least 3 bundled problems');
    for (const p of problems) {
        assert.ok(p.id, `problem missing id: ${JSON.stringify(p)}`);
        assert.ok(p.name, `problem ${p.id} missing name`);
        assert.ok(p.difficulty, `problem ${p.id} missing difficulty`);
    }
});

test('ProblemLoader: getProblem returns full definition for a known id', () => {
    const loader = new ProblemLoader();
    const car = loader.getProblem('car-registration');
    assert.ok(car, 'car-registration should exist');
    assert.ok(car.description, 'should include description');
    assert.ok(car.graph?.components?.length, 'should include components');
    assert.ok(Array.isArray(car.testCases) && car.testCases.length > 0, 'should include test cases');
});

test('ProblemLoader: getProblem returns null for unknown id', () => {
    const loader = new ProblemLoader();
    assert.equal(loader.getProblem('does-not-exist'), null);
});

test('ProblemLoader: every bundled problem has a valid graph + testCases', () => {
    const loader = new ProblemLoader();
    for (const { id } of loader.getProblems()) {
        const p = loader.getProblem(id);
        assert.ok(p.graph?.components?.length, `${id} has no components`);
        assert.ok(Array.isArray(p.testCases), `${id} has no testCases array`);
        for (const tc of p.testCases) {
            assert.ok(tc.name, `${id}: test case missing name`);
            assert.ok(Array.isArray(tc.steps), `${id}: test case "${tc.name}" missing steps`);
        }
    }
});
