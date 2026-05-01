import { describe, expect, it } from 'vitest';
import { canManageAgent, isEnrollmentPending, maintenanceAction, maintenanceActionLabel } from './agentActions';
describe('GIVEN agent management row action state', () => {
    it('WHEN enrollment is pending THEN approval actions are available', () => {
        expect(isEnrollmentPending({ approved: false, rejected: false })).toBe(true);
    });
    it('WHEN enrollment is approved or rejected THEN approval actions are hidden', () => {
        expect(isEnrollmentPending({ approved: true, rejected: false })).toBe(false);
        expect(isEnrollmentPending({ approved: false, rejected: true })).toBe(false);
    });
    it('WHEN an agent is approved THEN management actions are available unless it is rejected', () => {
        expect(canManageAgent({ approved: true, rejected: false })).toBe(true);
        expect(canManageAgent({ approved: false, rejected: false })).toBe(false);
        expect(canManageAgent({ approved: false, rejected: true })).toBe(false);
    });
    it('WHEN maintenance state changes THEN the row action toggles between maintenance and resume', () => {
        expect(maintenanceAction({ maintenance: false })).toBe('maintenance');
        expect(maintenanceActionLabel({ maintenance: false })).toBe('Maintenance');
        expect(maintenanceAction({ maintenance: true })).toBe('resume');
        expect(maintenanceActionLabel({ maintenance: true })).toBe('Resume');
    });
});
