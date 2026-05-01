import { afterEach, describe, expect, it, vi } from 'vitest';
import { approveAgent, deleteAgent, pauseAgent, rejectAgent, requestAgentStatus, restartAgentCollectors, resumeAgent, setAgentInterval, setAgentProcessLimit, } from './api';
describe('GIVEN agent management API actions', () => {
    afterEach(() => {
        vi.unstubAllGlobals();
    });
    it('WHEN enrollment actions are requested THEN the expected backend endpoints are called', async () => {
        const fetchMock = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ ok: true }) });
        vi.stubGlobal('fetch', fetchMock);
        await approveAgent('agent-1');
        await rejectAgent('agent-1');
        await deleteAgent('agent-1');
        expect(fetchMock).toHaveBeenNthCalledWith(1, '/api/agents/agent-1/approve', { method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(2, '/api/agents/agent-1/reject', { method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(3, '/api/agents/agent-1', { method: 'DELETE' });
    });
    it('WHEN settings are changed THEN command payloads are sent to the agent endpoints', async () => {
        const fetchMock = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ ok: true }) });
        vi.stubGlobal('fetch', fetchMock);
        await setAgentInterval('agent-2', 30);
        await setAgentProcessLimit('agent-2', 12);
        await pauseAgent('agent-2');
        await resumeAgent('agent-2');
        await restartAgentCollectors('agent-2');
        await requestAgentStatus('agent-2');
        expect(fetchMock).toHaveBeenNthCalledWith(1, '/api/agents/agent-2/set_interval', {
            body: JSON.stringify({ interval_seconds: 30 }),
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(2, '/api/agents/agent-2/set_process_limit', {
            body: JSON.stringify({ limit: 12 }),
            headers: { 'Content-Type': 'application/json' },
            method: 'POST',
        });
        expect(fetchMock).toHaveBeenNthCalledWith(3, '/api/agents/agent-2/pause', { method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(4, '/api/agents/agent-2/resume', { method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(5, '/api/agents/agent-2/restart_collectors', { method: 'POST' });
        expect(fetchMock).toHaveBeenNthCalledWith(6, '/api/agents/agent-2/get_status', { method: 'POST' });
    });
});
