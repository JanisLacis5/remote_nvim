-- Ordinary editing and writes stay local; only saved disk contents are uploaded.
local file = vim.env.JANISNVIM_FILE
local helper = vim.env.JANISNVIM_HELPER
local session = vim.env.JANISNVIM_SESSION

local active, queued, failed = nil, false, false
local buffer = vim.api.nvim_get_current_buf()
local group = vim.api.nvim_create_augroup('JanisNvimSync', { clear = true })

local function sync()
  if not vim.api.nvim_buf_is_valid(buffer) or vim.api.nvim_buf_get_name(buffer) ~= file then
    vim.notify('Remote identity changed; reopen the original cached file to sync', vim.log.levels.ERROR)
    return
  end

  if active then
    queued = true
    return
  end

  local output = {}
  vim.b[buffer].janisnvim_sync = 'pending'

  active = vim.fn.jobstart({ helper, '--sync', session, file }, {
    stdout_buffered = true,
    stderr_buffered = true,

    on_stdout = function(_, lines)
      vim.list_extend(output, lines)
    end,

    on_stderr = function(_, lines)
      vim.list_extend(output, lines)
    end,

    on_exit = function(_, code)
      active = nil
      failed = code ~= 0
      if vim.api.nvim_buf_is_valid(buffer) then
        vim.b[buffer].janisnvim_sync = failed and 'failed' or 'synced'
      end

      vim.schedule(function()
        local message = table.concat(output, '\n'):gsub('\n+$', '')
        vim.notify(message ~= '' and message or 'Remote sync failed; local copy retained',
          failed and vim.log.levels.ERROR or vim.log.levels.INFO)
      end)

      if queued then
        queued = false
        sync()
      end
    end,
  })

  if active <= 0 then
    active, failed = nil, true
    vim.b[buffer].janisnvim_sync = 'failed'
    vim.notify('Cannot start sync helper; local copy retained', vim.log.levels.ERROR)
  end
end

vim.b[buffer].janisnvim_sync = 'local'
vim.api.nvim_create_autocmd('BufWritePost', {
  group = group, buffer = buffer,
  callback = function()
    if vim.api.nvim_buf_get_name(buffer) == file then
      sync()
    end
  end,
})

vim.api.nvim_buf_create_user_command(buffer, 'JanisNvimSync', sync, {})

vim.api.nvim_create_autocmd('VimLeavePre', {
  group = group,
  callback = function()
    -- Exit may wait for an outstanding save; keystrokes and normal saves never do.
    local deadline = vim.loop.hrtime() + 35 * 1e9
    while active and vim.loop.hrtime() < deadline do
      vim.fn.jobwait({ active }, 100)
    end

    if active or failed then
      io.stderr:write('Remote sync incomplete; local working copy retained: ' .. file .. '\n')
    end
  end,
})
